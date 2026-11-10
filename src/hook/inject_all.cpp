#include "hook/inject_all.h"

#include "hook/snapshot.h"
#include "render/gfx_snapshot.h"

#include "game/band_census.h"
#include "game/event_script.h"
#include "game/pillarbox.h"
#include "hook/gapwatch.h"
#include "game/wide_staging.h"
#include "game/wide_rand.h"
#include "game/map_rim.h"
#include "game/config_text.h"
#include "game/rdata_consts.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/fishing_text.h"
#include "game/layering.h"
#include "game/game_clock.h"
#include "game/gfx_texcache.h"
#include "game/gfx_clut.h"
#include "game/gfx_filter.h"
#include "game/cheats.h"
#include "game/psp_code.h"
#include "input/button_tests.h"
#include "input/sigma.h"
#include "game/gfx_flush.h"
#include "game/gfx_unpack.h"
#include "game/gfx_vram_ops.h"
#include "game/platform/gfx.h"
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
#include "game/scenario/clut_strip.h"
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
#include "game/widescreen.h"
#include "game/d3d_lines.h"
#include "game/music_loops.h"
#include "audio/mp3.h"
#include "game/music_seq.h"
#include "game/battle_actions.h"
#include "game/battle/battle_2.h"
#include "game/battle_misc.h"
#include "game/battle_sprites.h"
#include "game/battle/battle_5.h"
#include "game/battle/battle_4.h"
#include "game/inventory_ops.h"
#include "game/battle_phases.h"
#include "game/battle/battle_intro.h"
#include "game/draw_layers.h"
#include "game/psx_gpu.h"
#include "game/psx_gte.h"
#include "game/psx_gte_float.h"
#include "game/psx_gte_matrix.h"
#include "game/psx_gte_transform.h"
#include "game/draw_emit.h"
#include "game/core/title.h"
#include "game/draw_pass.h"
#include "game/msg_pool.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_immediate.h"
#include "game/field_frame.h"
#include "game/field/sprite_2.h"
#include "game/title_states.h"
#include "game/frame_callees.h"
#include "game/field_event.h"
#include "game/msgbox.h"
#include "game/window_task.h"
#include "game/sprite_pose.h"
#include "game/scenario/scena_16.h"
#include "game/mode_flow.h"
#include "game/core/transition.h"
#include "game/area_data_reload.h"
#include "game/glyph_draw.h"
#include "game/yes_no_layout.h"
#include "game/text/msg_box.h"
#include "game/menu/item_use.h"
#include "game/sound.h"
#include "game/sound_rest.h"
#include "game/sound_sp.h"
#include "game/d3d_draw.h"
#include "game/d3d_rest.h"
#include "game/tex_page.h"
#include "game/text/tex.h"
#include "game/platform/cell.h"
#include "game/battle_result.h"
#include "game/sprt_draw.h"
#include "game/field_misc.h"
#include "game/save_menu.h"
#include "game/field/save.h"
#include "game/quick_menu.h"
#include "game/field/field_5.h"
#include "game/field/event_op.h"
#include "game/menu_windows.h"
#include "game/shoulder_glyphs.h"
#include "game/display_setup.h"
#include "game/win_main.h"
#include "game/world_map.h"
#include "game/battle_draw.h"
#include "game/battle_window_draw.h"
#include "game/battle_windows.h"
#include "game/battle_win_states.h"
#include "game/battle_turn_steps.h"
#include "game/magic/magic_fx_3.h"
#include "game/magic/fx_disc_fan.h"
#include "game/menu_draw_helpers.h"
#include "game/shop/shop_win.h"
#include "game/menu/window_2.h"
#include "game/menu/menu_win.h"
#include "game/menu_lists.h"
#include "game/shop_states.h"
#include "game/shop/shop_mode.h"
#include "game/area/world_map_hud.h"
#include "game/area/area_29.h"
#include "game/field/sprite.h"
#include "game/field/encounter.h"
#include "game/effect/effect_2.h"
#include "game/event_leader.h"
#include "game/text/text.h"
#include "game/shop_states2.h"
#include "game/map_field_objects.h"
#include "game/core/task.h"
#include "game/magic/skill_steal.h"
#include "game/magic/magic_fx_4.h"
#include "game/magic/drain_orb_a.h"
#include "game/magic/buff_spike.h"
#include "game/magic/iceblast.h"
#include "game/magic/frost_ring.h"
#include "game/magic/fx_106.h"
#include "game/magic/fx_105.h"
#include "game/magic/fx_104.h"
#include "game/magic/revive.h"
#include "game/magic/purify.h"
#include "game/magic/leech_shell.h"
#include "game/magic/shield_spark.h"
#include "game/magic/barrier_ring.h"
#include "game/magic/barrier_line.h"
#include "game/magic/barrier.h"
#include "game/magic/myollnir.h"
#include "game/magic/lightning_bolt.h"
#include "game/magic/jolt_bolt.h"
#include "game/magic/blizzard_shard.h"
#include "game/magic/simoon_dust.h"
#include "game/magic/fx_spiral.h"
#include "game/magic/fx_funnel.h"
#include "game/magic/spell_sleep.h"
#include "game/magic/spell_ragnarok.h"
#include "game/magic/spell_depress.h"
#include "game/magic/spell_confuse.h"
#include "game/magic_s20.h"
#include "game/magic/magic_092.h"
#include "game/magic/magic_088.h"
#include "game/magic/magic_087.h"
#include "game/magic/paralyzer.h"
#include "game/magic/magic_hold.h"
#include "game/boss/head_cracker_rock.h"
#include "game/boss/head_cracker.h"
#include "game/magic/magic_111.h"
#include "game/magic/magic_002_ball.h"
#include "game/magic/shadow_mote.h"
#include "game/magic/thunder_clap_bolt.h"
#include "game/magic/doom_breath_orb.h"
#include "game/magic/corona_ray.h"
#include "game/core/main_cannon.h"
#include "game/magic/breath_mote.h"
#include "game/magic/breath_beam.h"
#include "game/magic/whelp_breath_flames.h"
#include "game/magic/burn.h"
#include "game/battle/dragon_breath_beam.h"
#include "game/magic/magic_117.h"
#include "game/magic/magic_115.h"
#include "game/magic/magic_114.h"
#include "game/magic/venom_ring.h"
#include "game/magic/magic_fx_2.h"
#include "game/magic/magic_fx.h"
#include "game/magic/kaiser_ring.h"
#include "game/magic/kaiser_pillar.h"
#include "game/magic/kaiser.h"
#include "game/magic/holocaust_beam.h"
#include "game/magic/utmost_attack.h"
#include "game/magic/rotten_breath_mote.h"
#include "game/magic/bone_dance.h"
#include "game/magic/magic_213_mote.h"
#include "game/magic/pentagram.h"
#include "game/magic/white_flag.h"
#include "game/magic/nue_stomp_child.h"
#include "game/field/jump_child.h"
#include "game/magic/magic_020_lead.h"
#include "game/magic/magic_008_grow.h"
#include "game/magic/magic_008_dash.h"
#include "game/magic/magic_008.h"
#include "game/magic/magic_019.h"
#include "game/magic/magic_018_row_54.h"
#include "game/magic/magic_018_row_53.h"
#include "game/magic/magic_018_row_5.h"
#include "game/magic/magic_017_image.h"
#include "game/magic_s02.h"
#include "game/magic/super_combo.h"
#include "game/magic/elem_strike.h"
#include "game/magic/mind_sword_blade.h"
#include "game/magic/chlorine.h"
#include "game/magic/blitz_bolt.h"
#include "game/magic/ward_mote.h"
#include "game/magic/evil_eye_beam.h"
#include "game/magic/berserk.h"
#include "game/battle/counter_mark.h"
#include "game/magic/snap_wave.h"
#include "game/magic/kick_image.h"
#include "game/magic/war_shout_mote.h"
#include "game/magic/focus_aura.h"
#include "game/magic/enlighten_rays.h"
#include "game/magic/bonebreak_mote.h"
#include "game/magic/sudden_death_child.h"
#include "game/magic/tornado_mote.h"
#include "game/magic/sanctuary_mote.h"
#include "game/magic/pollen_mote.h"
#include "game/magic/elem_breath_mote.h"
#include "game/magic/dream_breath_mote.h"
#include "game/magic/bone_dart_shaft.h"
#include "game/magic/sacrifice_actor.h"
#include "game/magic/ovum.h"
#include "game/magic/lavaburst_child.h"
#include "game/magic/howling_child.h"
#include "game/magic/ebonfire_ring.h"
#include "game/magic/ebonfire.h"
#include "game/magic/identify.h"
#include "game/magic/celerity_spark.h"
#include "game/magic/weretiger.h"
#include "game/magic/tsunami_wave.h"
#include "game/magic/steal.h"
#include "game/magic/sparkle.h"
#include "game/magic/influence_right.h"
#include "game/magic/foretell.h"
#include "game/magic/chill_ray.h"
#include "game/magic/wall_of_fire_child.h"
#include "game/magic/eye_beam.h"
#include "game/magic/mighty_chop_blade.h"
#include "game/magic/accession_spark.h"
#include "game/magic/accession_ring.h"
#include "game/magic/accession_orb.h"
#include "game/magic/accession_ctl.h"
#include "game/magic/accession_bolt.h"
#include "game/magic/accession.h"
#include "game/magic/transfer_mote.h"
#include "game/magic/timed_blow.h"
#include "game/magic/monopolize_mote.h"
#include "game/magic/magic_159_child.h"
#include "game/magic/charm_mote.h"
#include "game/magic/magic_ball.h"
#include "game/magic/intimidate_trail.h"
#include "game/magic/aura_breath_dome.h"
#include "game/magic/magma_breath_record.h"
#include "game/battle/battle_fx_2.h"
#include "game/magic/combustion_flash.h"
#include "game/battle/combustion_mote.h"
#include "game/battle/combustion_glow.h"
#include "game/magic/geo_breath_child.h"
#include "game/magic/combustion.h"
#include "game/magic/last_resort_beam.h"
#include "game/magic/cure_mote.h"
#include "game/magic/benediction_child.h"
#include "game/magic/meteor_strike_rock.h"
#include "game/magic/magic_225_veil.h"
#include "game/area/area_11.h"
#include "game/area/area_26.h"
#include "game/area/area_23.h"
#include "game/area/area_22.h"
#include "game/area/area_21.h"
#include "game/area/area_20.h"
#include "game/area/area_19.h"
#include "game/area/area_18.h"
#include "game/area/area_16.h"
#include "game/scenario/scena_00.h"
#include "game/field/field.h"
#include "game/scena_calls.h"
#include "game/scenario/scena_call_5.h"
#include "game/scenario/scena_call_4.h"
#include "game/scenario/scena_call_3.h"
#include "game/scenario/scena_call_2.h"
#include "game/scenario/scena_call.h"
#include "game/scenario/scena_15.h"
#include "game/scenario/scena_14.h"
#include "game/scenario/scena_13.h"
#include "game/scenario/scena_12.h"
#include "game/scenario/scena_10_2.h"
#include "game/scenario/scena_10.h"
#include "game/scenario/scena_09.h"
#include "game/scenario/scena_08.h"
#include "game/scenario/scena_07.h"
#include "game/scenario/scena_06_2.h"
#include "game/scenario/scena_06.h"
#include "game/scenario/scena_05.h"
#include "game/scenario/scena_04.h"
#include "game/scenario/scena_02.h"
#include "game/scenario/scena_01.h"
#include "game/scenario/scena_11.h"
#include "game/scenario/scena_04_2.h"
#include "game/scenario/scena_03.h"
#include "game/scenario/scena_01_2.h"
#include "game/scenario/scena_12_2.h"
#include "game/scena_sc9a.h"
#include "game/area/area_15.h"
#include "game/area/area_13.h"
#include "game/area/area_12.h"
#include "game/area/area_10.h"
#include "game/area/area_08.h"
#include "game/area/area_07.h"
#include "game/area/area_05_2.h"
#include "game/area/area_05.h"
#include "game/area/area_03.h"
#include "game/area/area_02.h"
#include "game/area/area_01.h"
#include "game/area/area_00.h"
#include "game/scenario/scena_06_3.h"
#include "game/area/area_37.h"
#include "game/area/area_35.h"
#include "game/area/area_34.h"
#include "game/area/area_32.h"
#include "game/area/area_28.h"
#include "game/area/area_27.h"
#include "game/area/area_41.h"
#include "game/area/area_40.h"
#include "game/area/area_39.h"
#include "game/scenario/scena_08_2.h"
#include "game/scenario/scena_07_2.h"
#include "game/scenario/scena_05_2.h"
#include "game/field/char.h"
#include "game/area/area_47.h"
#include "game/area/area_46.h"
#include "game/area/area_45.h"
#include "game/area/area_43.h"
#include "game/area/area_42.h"
#include "game/scena_sc9b.h"
#include "game/scenario/scena_10_3.h"
#include "game/scenario/scena_09_2.h"
#include "game/area_w1c.h"
#include "game/area/area_52.h"
#include "game/area/area_51.h"
#include "game/area/area_50.h"
#include "game/area/area_49.h"
#include "game/area/area_48.h"
#include "game/area/area_87.h"
#include "game/area/area_86.h"
#include "game/area/area_85.h"
#include "game/scena_sc15.h"
#include "game/scenario/scena_19.h"
#include "game/scenario/scena_18.h"
#include "game/scenario/scena_17.h"
#include "game/scenario/scena_16_2.h"
#include "game/scenario/scena_15_2.h"
#include "game/scenario/scena_02_2.h"
#include "game/scenario/scena_14_2.h"
#include "game/scenario/scena_13_2.h"
#include "game/core/sound.h"
#include "game/scenario_harness_fh.h"
#include "game/scenario_harness_ekh.h"
#include "game/effect_1e.h"
#include "game/area/area_67.h"
#include "game/area/area_84.h"
#include "game/area/area_82.h"
#include "game/area/area_81.h"
#include "game/area/area_80.h"
#include "game/area/area_79.h"
#include "game/area/area_78.h"
#include "game/area/area_77.h"
#include "game/area/area_65.h"
#include "game/area/area_63.h"
#include "game/area/area_62.h"
#include "game/area/area_61.h"
#include "game/area/area_59.h"
#include "game/area/area_57.h"
#include "game/area/area_56.h"
#include "game/area/area_55.h"
#include "game/area/area_99.h"
#include "game/area/area_98.h"
#include "game/area/area_97.h"
#include "game/area/area_96.h"
#include "game/area/area_95.h"
#include "game/area/area_103.h"
#include "game/area/area_100.h"
#include "game/area/area_106.h"
#include "game/area/area_105.h"
#include "game/area/area_104.h"
#include "game/area/area_94.h"
#include "game/area/area_92.h"
#include "game/area/area_91.h"
#include "game/area_w1f.h"
#include "game/area/area_75.h"
#include "game/area/area_74.h"
#include "game/area/area_73.h"
#include "game/area/area_72.h"
#include "game/area/area_71.h"
#include "game/area/area_69.h"
#include "game/area/area_68.h"
#include "game/area/area_119.h"
#include "game/area/area_118.h"
#include "game/area/area_117.h"
#include "game/area/area_115_2.h"
#include "game/area_w3c.h"
#include "game/area/area_133.h"
#include "game/area/area_132.h"
#include "game/area/area_131.h"
#include "game/area/area_130.h"
#include "game/area/area_128.h"
#include "game/area/area_127.h"
#include "game/area/area_125.h"
#include "game/area/area_124.h"
#include "game/area/area_121.h"
#include "game/area/area_120.h"
#include "game/area/area_146.h"
#include "game/area/area_145.h"
#include "game/area/area_144.h"
#include "game/area/area_143.h"
#include "game/area/area_142.h"
#include "game/area/area_141.h"
#include "game/area/area_140.h"
#include "game/area/area_139.h"
#include "game/area/area_136.h"
#include "game/area_w3d.h"
#include "game/area/area_135.h"
#include "game/area/area_134.h"
#include "game/area/area_151.h"
#include "game/area/area_149.h"
#include "game/area/area_148.h"
#include "game/area/area_115.h"
#include "game/area/area_112.h"
#include "game/area/area_111.h"
#include "game/area/area_108.h"
#include "game/area_w4a.h"
#include "game/area/area_167.h"
#include "game/area/area_155.h"
#include "game/area/area_154.h"
#include "game/area/area_153.h"
#include "game/area/area_152.h"
#include "game/area/area_174.h"
#include "game/area/area_173.h"
#include "game/area/area_187.h"
#include "game/area/area_186.h"
#include "game/area/area_185.h"
#include "game/area/area_184.h"
#include "game/area/area_183.h"
#include "game/area/area_182.h"
#include "game/area/area_181.h"
#include "game/area/area_180.h"
#include "game/area/area_179.h"
#include "game/area/area_178.h"
#include "game/area/area_177.h"
#include "game/area/area_176.h"
#include "game/area/area_175.h"
#include "game/area/area_172.h"
#include "game/area/area_171.h"
#include "game/area/area_170.h"
#include "game/area/area_169.h"
#include "game/area/area_168.h"
#include "game/area/area_191.h"
#include "game/area/area_189.h"
#include "game/area/area_188.h"
#include "game/area/area_199.h"
#include "game/area/area_198.h"
#include "game/area/area_197.h"
#include "game/area/area_196.h"
#include "game/area/area_193.h"
#include "game/area/area_192.h"
#include "game/area/bare_ret.h"
#include "game/boss/boss_worker.h"
#include "game/boss/boss_torast.h"
#include "game/boss/boss_operator.h"
#include "game/boss/boss_kassen.h"
#include "game/boss/boss_galtel.h"
#include "game/boss/boss_engineer.h"
#include "game/boss/boss_doksen.h"
#include "game/boss/boss_07.h"
#include "game/boss/boss_04.h"
#include "game/boss/boss_nue.h"
#include "game/boss/boss_gary.h"
#include "game/boss/boss_02.h"
#include "game/boss/boss_01.h"
#include "game/boss/boss_sunder.h"
#include "game/boss/boss_balio.h"
#include "game/boss/boss_amalgam.h"
#include "game/boss/boss_08.h"
#include "game/boss/boss_zig.h"
#include "game/boss/boss_stallion.h"
#include "game/boss/boss_nina.h"
#include "game/boss/boss_bully.h"
#include "game/boss/boss_beyd.h"
#include "game/boss/boss_24.h"
#include "game/boss/boss_23.h"
#include "game/boss/boss_sample_4.h"
#include "game/boss/boss_sample_2.h"
#include "game/boss/boss_chimera.h"
#include "game/boss/boss_arwan_fx.h"
#include "game/boss/boss_arwan.h"
#include "game/boss/boss_ammonite.h"
#include "game/boss/boss_53.h"
#include "game/boss/boss_38.h"
#include "game/boss/boss_gaist.h"
#include "game/boss/boss_elder.h"
#include "game/boss/boss_angler_fx.h"
#include "game/boss/boss_angler.h"
#include "game/boss/boss_36.h"
#include "game/boss/boss_35.h"
#include "game/boss/boss_shroom.h"
#include "game/boss/boss_d_lord_fx.h"
#include "game/boss/boss_d_lord.h"
#include "game/boss/boss_55.h"
#include "game/boss/boss_52.h"
#include "game/boss_sg.h"
#include "game/boss/boss_weretigr_fx.h"
#include "game/boss/boss_weretigr.h"
#include "game/boss/boss_mikba.h"
#include "game/boss/boss_gisshan.h"
#include "game/boss/boss_garr_2.h"
#include "game/boss/boss_dolphin.h"
#include "game/boss/boss_d_zombie.h"
#include "game/boss/boss_32.h"
#include "game/boss/boss_31.h"
#include "game/boss/boss_myria.h"
#include "game/boss/boss_gazer_fx.h"
#include "game/boss/boss_gazer.h"
#include "game/boss/boss_28.h"
#include "game/boss/boss_25.h"
#include "game/boss_harness_eh.h"
#include "game/battle_e7.h"
#include "game/battle/battle_result.h"
#include "game/battle/battle_loss.h"
#include "game/battle_e5.h"
#include "game/battle/battle_banner.h"
#include "game/boss/boss_myria_fx.h"
#include "game/battle/battle_obj.h"
#include "game/field_s.h"
#include "game/field_e2.h"
#include "game/field/area_map.h"
#include "game/field/field_3.h"
#include "game/field/field_4.h"
#include "game/field/field_6.h"
#include "game/field_o.h"
#include "game/menu/menu.h"
#include "game/field/move_script.h"
#include "game/field/move_cmd.h"
#include "game/field/event_script.h"
#include "game/effect_gte.h"
#include "game/effect/effect_gte.h"
#include "game/effect_1f.h"
#include "game/effect_1b.h"
#include "game/effect/effect_kind_17.h"
#include "game/effect/effect_kind_11.h"
#include "game/effect/effect_kind_06.h"
#include "game/effect_1g.h"
#include "game/effect_1d.h"
#include "game/effect/effect_kind_27.h"
#include "game/effect/effect_kind_21.h"
#include "game/effect_1a.h"
#include "game/effect/world_map.h"
#include "game/effect/effect_kind_0f.h"
#include "game/effect/effect_kind_09.h"
#include "game/effect_2g.h"
#include "game/effect/effect_kind_5f.h"
#include "game/effect/effect_kind_5d.h"
#include "game/effect/effect_kind_54.h"
#include "game/effect/effect_shards.h"
#include "game/effect/effect_kind_34.h"
#include "game/effect/effect_kind_20.h"
#include "game/effect/effect_kind_1d.h"
#include "game/effect/effect_kind_1c.h"
#include "game/effect/effect_kind_3d.h"
#include "game/effect/effect_kind_3b.h"
#include "game/effect/effect_kind_38.h"
#include "game/effect/effect_kind_35.h"
#include "game/effect/effect_kind_33.h"
#include "game/effect/effect_kind_2f.h"
#include "game/effect_2d.h"
#include "game/effect/effect_kind_48.h"
#include "game/effect/effect_kind_46.h"
#include "game/effect/effect_kind_45.h"
#include "game/effect_2c.h"
#include "game/effect/effect_kind_44.h"
#include "game/effect/effect_kind_43.h"
#include "game/effect_2f.h"
#include "game/effect/effect_kind_66.h"
#include "game/effect/effect_kind_51.h"
#include "game/effect_2e.h"
#include "game/effect/effect_kind_4e.h"
#include "game/effect/effect_kind_4d.h"
#include "game/effect_2a.h"
#include "game/effect/effect_specks.h"
#include "game/effect/effect_kind_2e.h"
#include "game/effect/effect_kind_2d.h"
#include "game/effect/effect_kind_2b.h"
#include "game/effect/effect_kind_2a.h"
#include "game/effect/effect_kind_29.h"
#include "game/effect/effect_kind_28.h"
#include "game/effect/effect_kind_6e.h"
#include "game/effect/effect_kind_69.h"
#include "game/effect/effect_kind_65.h"
#include "game/effect/effect_kind_63.h"
#include "game/effect/effect_kind_82.h"
#include "game/effect/effect_kind_81.h"
#include "game/effect/effect_kind_80.h"
#include "game/effect/effect_kind_7d.h"
#include "game/effect/effect_kind_78.h"
#include "game/effect/effect_kind_77.h"
#include "game/effect/effect_kind_87.h"
#include "game/effect/effect_kind_85.h"
#include "game/effect/effect_kind_84.h"
#include "game/effect/effect_kind_83.h"
#include "game/effect/effect_kind_18_sub_12.h"
#include "game/effect/effect_kind_18_sub_11.h"
#include "game/effect/effect_kind_18_sub_10.h"
#include "game/effect/effect_kind_73.h"
#include "game/effect/effect_kind_6f.h"
#include "game/effect/effect_debris.h"
#include "game/effect_3a.h"
#include "game/effect/effect_kind_64.h"
#include "game/effect/effect_kind_8f.h"
#include "game/effect/effect_kind_8e.h"
#include "game/effect/effect_kind_8d.h"
#include "game/effect/effect_kind_ba.h"
#include "game/effect/effect_kind_b9.h"
#include "game/effect/effect_kind_b1.h"
#include "game/effect/effect_kind_ae.h"
#include "game/effect/effect_kind_ac.h"
#include "game/effect/effect_kind_ab.h"
#include "game/effect/effect_kind_9a.h"
#include "game/effect/effect_kind_97.h"
#include "game/effect/effect_kind_95.h"
#include "game/effect/effect_kind_94.h"
#include "game/effect/effect_kind_91.h"
#include "game/effect/effect_kind_a8.h"
#include "game/effect/effect_kind_a0.h"
#include "game/effect/effect_kind_9e.h"
#include "game/effect/effect_kind_9c.h"
#include "game/effect_4b.h"
#include "game/effect/effect_kind_9d.h"
#include "game/effect/effect_kind_8c.h"
#include "game/effect/effect_kind_8a.h"
#include "game/effect/effect_kind_88.h"
#include "game/effect/effect_kind_18_sub_2b.h"
#include "game/effect/effect_kind_18_sub_32.h"
#include "game/effect/effect_kind_18_sub_31.h"
#include "game/effect/effect_kind_18_sub_30.h"
#include "game/effect/effect_kind_18_sub_2f.h"
#include "game/effect/effect_kind_18_sub_2e.h"
#include "game/effect/effect_kind_18_sub_2d.h"
#include "game/effect/effect_kind_18_sub_2c.h"
#include "game/effect/effect_kind_18_sub_59.h"
#include "game/effect/effect_kind_18_sub_55.h"
#include "game/effect/effect_kind_18_sub_51.h"
#include "game/effect/effect_kind_18_sub_45.h"
#include "game/effect/effect_kind_18.h"
#include "game/effect/effect_kind_18_sub_0f.h"
#include "game/effect/effect_kind_18_sub_0e.h"
#include "game/effect/effect_kind_18_sub_0d.h"
#include "game/effect/effect_kind_18_sub_0c.h"
#include "game/effect/effect_kind_18_sub_42.h"
#include "game/effect/effect_kind_18_sub_2a.h"
#include "game/effect/effect_kind_18_sub_29.h"
#include "game/effect/effect_kind_18_sub_27.h"
#include "game/effect/effect_kind_18_sub_3f.h"
#include "game/effect/effect_kind_18_sub_26.h"
#include "game/effect/effect_kind_18_sub_23.h"
#include "game/effect/effect_kind_18_sub_22.h"
#include "game/effect/effect_kind_18_sub_21.h"
#include "game/effect/effect_kind_18_sub_1c.h"
#include "game/effect/effect_kind_18_sub_17.h"
#include "game/effect/effect_kind_18_sub_65.h"
#include "game/effect/effect_kind_18_sub_64.h"
#include "game/effect/effect_kind_18_sub_61.h"
#include "game/effect/effect_kind_18_sub_5d.h"
#include "game/effect/effect_kind_18_sub_5b.h"
#include "game/effect/effect_kind_18_sub_40.h"
#include "game/effect/effect_kind_18_sub_3b.h"
#include "game/effect/effect_kind_18_sub_35.h"
#include "game/effect/effect_kind_18_sub_33.h"
#include "game/rest_0a.h"
#include "game/rest_1b.h"
#include "game/rest_1e.h"
#include "game/rest_1a.h"
#include "game/field/member.h"
#include "game/rest_1f.h"
#include "game/field/leader_panel.h"
#include "game/field/field_core.h"
#include "game/rest_1d.h"
#include "game/field/party_action.h"
#include "game/rest_2a.h"
#include "game/scenario/linked_object.h"
#include "game/field/mode_11.h"
#include "game/rest_2d.h"
#include "game/menu/master_quit.h"
#include "game/field/field_menu_status.h"
#include "game/field/field_menu.h"
#include "game/field/field_ability.h"
#include "game/rest_2e.h"
#include "game/rest_2h.h"
#include "game/shop/shop_win_2.h"
#include "game/platform/d_input.h"
#include "game/menu/menu_list.h"
#include "game/battle/battle_menu_win.h"
#include "game/rest_2f.h"
#include "game/menu/window.h"
#include "game/menu/config_menu.h"
#include "game/field/field_ability_2.h"
#include "game/rest_3f.h"
#include "game/rest_2c.h"
#include "game/shop/shop_trade.h"
#include "game/shop/shop_resist.h"
#include "game/shop/shop_browse.h"
#include "game/shop/shared_list.h"
#include "game/shop/field_save.h"
#include "game/menu/master_talk_2.h"
#include "game/menu/master_talk.h"
#include "game/field/party_form.h"
#include "game/rest_3g.h"
#include "game/field/quake.h"
#include "game/effect/effect_kind_18_sub_44.h"
#include "game/effect/effect_kind_0f_2.h"
#include "game/area/area_109.h"
#include "game/field/field_2.h"
#include "game/field/field_trigger_01.h"
#include "game/community/commu_sim.h"
#include "game/rest_2b.h"
#include "game/minigame/shisu.h"
#include "game/menu/menu_2.h"
#include "game/field/field_7.h"
#include "game/field/event_cond.h"
#include "game/battle/battle_extra.h"
#include "game/area/world_map_33.h"
#include "game/battle/enemy_op.h"
#include "game/battle/battle_task.h"
#include "game/battle/battle_fx.h"
#include "game/battle/battle_action.h"
#include "game/battle/battle.h"
#include "game/rest_3e.h"
#include "game/battle/dragon_cmd.h"
#include "game/effect/effect.h"
#include "game/battle/battle_item_cmd.h"
#include "game/battle/battle_3.h"
#include "game/rest_4f.h"
#include "game/rest_4e.h"
#include "game/community/commu_music.h"
#include "game/rest_4d.h"
#include "game/community/commu_name.h"
#include "game/community/commu.h"
#include "game/rest_4b.h"
#include "game/community/commu_tail.h"
#include "game/community/commu_board.h"
#include "game/shell.h"
#include "game/platform/magic_080.h"
#include "game/platform/file.h"
#include "game/platform/cfg.h"
#include "game/platform/music.h"
#include "game/platform/gte.h"
#include "game/platform/gpu_2.h"
#include "game/platform/gpu.h"
#include "game/platform/d3d.h"
#include "game/mode_rest.h"
#include "game/crt_rest.h"
#include "game/menu/item_trade.h"
#include "game/menu/char.h"
#include "hook/detour.h"
#include "hook/exe_own.h"

// The start-up self-tests of the groups whose own files the source-layout round retired:
// each is defined in its group's <folder>/<group>_fuzz.cpp and called below where the
// group's _Inject() called it.
// retire_shells: self-tests begin (tools/retire_shells.py keeps these lines)
namespace area_011 { void SelfTest(); }
namespace area_backdrop { void SelfTest(); }
namespace area_cell_hook { void SelfTest(); }
namespace area_w0a { void SelfTest(); }
namespace area_w0b { void SelfTest(); }
namespace area_w0c { void SelfTest(); }
namespace area_w1a { void SelfTest(); }
namespace area_w1b { void SelfTest(); }
namespace area_w1d { void SelfTest(); }
namespace area_w1e { void SelfTest(); }
namespace area_w2a { void SelfTest(); }
namespace area_w2b { void SelfTest(); }
namespace area_w2c { void SelfTest(); }
namespace area_w2d { void SelfTest(); }
namespace area_w2e { void SelfTest(); }
namespace area_w2f { void SelfTest(); }
namespace area_w3a { void SelfTest(); }
namespace area_w3b { void SelfTest(); }
namespace area_w3e { void SelfTest(); }
namespace area_w3f { void SelfTest(); }
namespace area_w3g { void SelfTest(); }
namespace area_w4b { void SelfTest(); }
namespace area_w4c { void SelfTest(); }
namespace area_w4d { void SelfTest(); }
namespace area_w4e { void SelfTest(); }
namespace area_w4f { void SelfTest(); }
namespace battle_actor_copies { void SelfTest(); }
namespace battle_damage { void SelfTest(); }
namespace battle_e1 { void SelfTest(); }
namespace battle_e2 { void SelfTest(); }
namespace battle_e3 { void SelfTest(); }
namespace battle_e4 { void SelfTest(); }
namespace battle_e6 { void SelfTest(); }
namespace battle_flow { void SelfTest(); }
namespace battle_fx_tasks { void SelfTest(); }
namespace battle_items { void SelfTest(); }
namespace battle_menu_states { void SelfTest(); }
namespace battle_obj_states { void SelfTest(); }
namespace battle_odds { void SelfTest(); }
namespace battle_setup { void SelfTest(); }
namespace boss_h { void SelfTest(); }
namespace boss_sa { void SelfTest(); }
namespace boss_sb { void SelfTest(); }
namespace boss_sc { void SelfTest(); }
namespace boss_sd { void SelfTest(); }
namespace boss_se { void SelfTest(); }
namespace boss_sf { void SelfTest(); }
namespace boss_sh { void SelfTest(); }
namespace boss_si { void SelfTest(); }
namespace boss_sj { void SelfTest(); }
namespace boss_spawn { void SelfTest(); }
namespace char_stats { void SelfTest(); }
namespace display_env { void SelfTest(); }
namespace effect_1c { void SelfTest(); }
namespace effect_1d { void SelfTest(); }
namespace effect_1f { void SelfTest(); }
namespace effect_2a { void SelfTest(); }
namespace effect_2b { void SelfTest(); }
namespace effect_2e { void SelfTest(); }
namespace effect_2f { void SelfTest(); }
namespace effect_3b { void SelfTest(); }
namespace effect_3c { void SelfTest(); }
namespace effect_3d { void SelfTest(); }
namespace effect_4a { void SelfTest(); }
namespace effect_4c { void SelfTest(); }
namespace effect_4d { void SelfTest(); }
namespace effect_4e { void SelfTest(); }
namespace effect_4f { void SelfTest(); }
namespace effect_5a { void SelfTest(); }
namespace effect_5b { void SelfTest(); }
namespace effect_5c { void SelfTest(); }
namespace effect_5d { void SelfTest(); }
namespace effect_5e { void SelfTest(); }
namespace effect_5f { void SelfTest(); }
namespace effect_5g { void SelfTest(); }
namespace effect_6a { void SelfTest(); }
namespace effect_6b { void SelfTest(); }
namespace effect_6c { void SelfTest(); }
namespace effect_6d { void SelfTest(); }
namespace enemy_ai_ops { void SelfTest(); }
namespace event_objs { void SelfTest(); }
namespace field_c1 { void SelfTest(); }
namespace field_c2 { void SelfTest(); }
namespace field_c3 { void SelfTest(); }
namespace field_e1 { void SelfTest(); }
namespace field_hidden { void SelfTest(); }
namespace field_modes { void SelfTest(); }
namespace game_last { void SelfTest(); }
namespace item_use { void SelfTest(); }
namespace magic_c1 { void SelfTest(); }
namespace magic_c2 { void SelfTest(); }
namespace magic_c3 { void SelfTest(); }
namespace magic_engine { void SelfTest(); }
namespace magic_fx_reached { void SelfTest(); }
namespace magic_lib { void SelfTest(); }
namespace magic_s01 { void SelfTest(); }
namespace magic_s03 { void SelfTest(); }
namespace magic_s04 { void SelfTest(); }
namespace magic_s05 { void SelfTest(); }
namespace magic_s06 { void SelfTest(); }
namespace magic_s07 { void SelfTest(); }
namespace magic_s08 { void SelfTest(); }
namespace magic_s09 { void SelfTest(); }
namespace magic_s10 { void SelfTest(); }
namespace magic_s11 { void SelfTest(); }
namespace magic_s12 { void SelfTest(); }
namespace magic_s13 { void SelfTest(); }
namespace magic_s14 { void SelfTest(); }
namespace magic_s15 { void SelfTest(); }
namespace magic_s16 { void SelfTest(); }
namespace magic_s17 { void SelfTest(); }
namespace magic_s18 { void SelfTest(); }
namespace magic_s19 { void SelfTest(); }
namespace magic_s21 { void SelfTest(); }
namespace magic_s22 { void SelfTest(); }
namespace magic_s23 { void SelfTest(); }
namespace magic_s24 { void SelfTest(); }
namespace magic_s25 { void SelfTest(); }
namespace magic_s26 { void SelfTest(); }
namespace magic_s27 { void SelfTest(); }
namespace magic_s28 { void SelfTest(); }
namespace magic_s29 { void SelfTest(); }
namespace magic_s30 { void SelfTest(); }
namespace magic_s31 { void SelfTest(); }
namespace magic_s32 { void SelfTest(); }
namespace magic_s33 { void SelfTest(); }
namespace magic_s34 { void SelfTest(); }
namespace magic_s35 { void SelfTest(); }
namespace magic_s36 { void SelfTest(); }
namespace magic_s37 { void SelfTest(); }
namespace magic_s38 { void SelfTest(); }
namespace magic_steal { void SelfTest(); }
namespace map_scroll { void SelfTest(); }
namespace member_sprites { void SelfTest(); }
namespace mode_states { void SelfTest(); }
namespace move_cmds { void SelfTest(); }
namespace psx_rest { void SelfTest(); }
namespace rest_0a { void SelfTest(); }
namespace rest_1a { void SelfTest(); }
namespace rest_1b { void SelfTest(); }
namespace rest_1c { void SelfTest(); }
namespace rest_1d { void SelfTest(); }
namespace rest_1e { void SelfTest(); }
namespace rest_1f { void SelfTest(); }
namespace rest_1g { void SelfTest(); }
namespace rest_2a { void SelfTest(); }
namespace rest_2g { void SelfTest(); }
namespace rest_3a { void SelfTest(); }
namespace rest_3b { void SelfTest(); }
namespace rest_3c { void SelfTest(); }
namespace rest_3d { void SelfTest(); }
namespace rest_4a { void SelfTest(); }
namespace rest_4c { void SelfTest(); }
namespace scena_sc0 { void SelfTest(); }
namespace scena_sc1 { void SelfTest(); }
namespace scena_sc11 { void SelfTest(); }
namespace scena_sc12 { void SelfTest(); }
namespace scena_sc13 { void SelfTest(); }
namespace scena_sc2 { void SelfTest(); }
namespace scena_sc3 { void SelfTest(); }
namespace scena_sc5 { void SelfTest(); }
namespace scena_sc6 { void SelfTest(); }
namespace scena_sc7 { void SelfTest(); }
namespace scena_se { void SelfTest(); }
namespace scena_sx { void SelfTest(); }
namespace scena_sx2 { void SelfTest(); }
namespace task_sched { void SelfTest(); }
namespace tex_cells { void SelfTest(); }
namespace window_kinds { void SelfTest(); }
namespace worldmap_area { void SelfTest(); }
// retire_shells: self-tests end

namespace bof3 {

void InjectAll() {
    rdata::Verify();            // first: the engine's copies of .rdata's constants against BOF3.exe's, before any fuzz reads one
    ExeOwn_CheckAtStart();      // the engine's own .data bytes (exe_own.h) against BOF3.exe's, while .data is still the PC's
    DrawPool_Reserve();         // DIV-0062: the draw-item pool's room below 16 MB, before anything else is placed
    SpriteRecords_Inject();     // first: its fuzz runs the original call tree, so none of it may be patched yet
    MapCells_Inject();          // likewise
    GameClock_Inject();         // DIV-0022: before WinMain first reads the clock
    MsgPool_Inject();           // before DatLoad_Inject, which may relocate the pool
    TextMsgBoxFromMsgPool_Inject();
    ConfigText_Inject();        // layout only; the text arrives with FIRST.DAT
    MenuVerbs_Inject();         // likewise
    DatLoad_Inject();
    TextAdvance_Inject();
    TextDraw_Inject();
    TextImmediate_Inject();
    GfxTexCache_Inject();
    GfxClut_Inject();
    GfxFlush_Inject();
    TitleFromGfxFlush_Inject();
    GfxUnpack_Inject();
    TitleFromGfxUnpack_Inject();
    GfxVramOps_Inject();
    Gfx_Inject();
    GfxFilter_Inject();
    Cheats_Inject();            // DIV-0045 / DIV-0046, BOF3X_EXP / BOF3X_ZENNY / BOF3X_STEAL: nothing when unset
    PspCode_Inject();           // DIV-0108: BOF3X_PSP, or the cache's base_policy; before the battle modules
    MenuFrame_Inject();
    DrawPass_Inject();          // before what it calls: it clones their originals
    SpriteDraw_Inject();        // likewise: before PsxGpu_Inject and DrawEmit_Inject
    SpriteOrder_Inject();
    DrawPool_Inject();
    Prim_Inject();
    TitleFromPrim_Inject();
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
    TitleFromDrawEmit_Inject();
    PsxGteMatrix_Inject();      // before what it calls: it clones their originals
    PsxGteTransform_Inject();   // likewise
    PsxGpu_Inject();
    PsxGte_Inject();
    PsxGteFloat_Inject();
    FieldFrame_Inject();        // every call of its clones re-aimed, the handler table swapped: order does not matter
    Sprite2_Inject();
    Kind2Object_Inject();       // likewise, its jump tables re-aimed
    AreaSlope_Inject();         // no calls out
    ModeTasks_Inject();         // every call of its clones re-aimed: order does not matter
    TransitionFromModeTasks_Inject();
    TitleFromModeTasks_Inject();
    ObjectKinds_Inject();       // every call of its clones re-aimed, the pace and fade tables swapped: order does not matter
    MapLayers_Inject();         // its fuzz stands recorders in for every callee, so any slot will do
    TitleStates_Inject();       // every call of its clones re-aimed: order does not matter
    TitleFromTitleStates_Inject();
    FieldBlocked_Inject();      // every call of its clones re-aimed at a recorder: order does not matter
    FrameCallees_Inject();      // every call of its clones re-aimed: order does not matter
    EffectEffectGteFromFrameCallees_Inject();
    EffectKind06FromFrameCallees_Inject();
    if (bof3::WantsShadow("field_modes")) field_modes::SelfTest();        // every call of its clones re-aimed, every table it reads swapped: order does not matter
    ClutStrip_Inject();
    if (bof3::WantsShadow("map_scroll")) map_scroll::SelfTest();         // likewise: every call of its clones re-aimed at a recorder
    FieldEvent_Inject();        // every call of its clones re-aimed at a recorder: any slot will do
    EventScript_Inject();       // its fuzz stands recorders in for every callee, so any slot will do
    MsgBox_Inject();            // every call of its clones re-aimed at a recorder, and every stack-built
                                // dispatch table's immediates too: order does not matter
    TextMsgBoxFromMsgbox_Inject();
    WindowTask_Inject();        // every call of its clones is re-aimed at a recorder and every
                                // stack-built table re-aimed in the copy, so order does not matter
    SpritePose_Inject();        // every call of its clones re-aimed at a recorder or at another of its
                                // own clones: order does not matter
    if (bof3::WantsShadow("move_cmds")) move_cmds::SelfTest();          // every call of its clones re-aimed at a recorder, its jump table
                                // relocated in the copy: order does not matter
    Scena16_Inject();
    ModeFlow_Inject();          // every call of its clones re-aimed at a recorder, its stack-built table
                                // re-aimed in the copy, the mode table swapped: order does not matter
    TransitionFromModeFlow_Inject();
    TitleFromModeFlow_Inject();
    AreaDataReload_Inject();    // DIV-0182: a copy of two areas' .data, taken before any scene: before the
                                // first Area_Enter, after nothing else
    GlyphDraw_Inject();         // every call of its clones re-aimed at a recorder, the device a fake:
                                // order does not matter
    YesNoLayout_Inject();       // patches only (DIV-0027): order does not matter
    if (bof3::WantsShadow("item_use")) item_use::SelfTest();           // every call of its clones re-aimed at a recorder, the handler table and
                                // four area descriptors swapped: order does not matter
    TextMsgBoxFromItemUse_Inject();
    MenuItemUse_Inject();
    TitleFromItemUse_Inject();
    Sound_Inject();             // every call of its clones re-aimed at a recorder, its three Win32 import
                                // operands moved onto recorders in the copies: order does not matter
    D3dDraw_Inject();           // every call of its clones re-aimed at a recorder, its four jump tables
                                // relocated in the copies, the device a fake: order does not matter
    if (bof3::WantsShadow("display_env")) display_env::SelfTest();        // every call of its clones re-aimed at a recorder or at another of its
                                // clones, the surfaces, viewport and material fakes: order does not matter
    TexPage_Inject();           // every call of its builder copies re-aimed at a recorder or at its own
                                // helper and converter copies, DirectDraw a fake: order does not matter
    if (bof3::WantsShadow("tex_cells")) tex_cells::SelfTest();          // every call of its builder copies re-aimed at a recorder or at its own
                                // helper copies, DirectDraw and the device fakes: order does not matter
    Tex_Inject();
    Cell_Inject();
    if (bof3::WantsShadow("event_objs")) event_objs::SelfTest();         // every call of its clones re-aimed at a recorder: order does not matter
    if (bof3::WantsShadow("char_stats")) char_stats::SelfTest();         // every call of its clones re-aimed at a recorder, its seven jump tables
                                // relocated in the copies, the trait lists swapped: order does not matter
    if (bof3::WantsShadow("member_sprites")) member_sprites::SelfTest();     // every call of its clones re-aimed at a recorder, its two dispatch tables
                                // swapped and three jump tables relocated in the copies: order does not matter
    SprtDraw_Inject();          // its copies are of Capcom's bytes, which nothing but its own Inject
                                // patches; every call of them re-aimed at a recorder, the device a
                                // fake: order does not matter
    FieldMisc_Inject();         // every call of its clones re-aimed at a recorder, its jump table
                                // relocated in the copy, the device a fake: order does not matter
    SaveMenu_Inject();          // every call of its clones re-aimed at a recorder, its jump tables swapped
                                // for recorders and Shop_Equip's relocated in the copy: order does not matter
    Save_Inject();
    Field5_Inject();
    EventOp_Inject();
    MenuWindows_Inject();       // after MenuVerbs and YesNoLayout, whose patch sites it reads back (DIV-0018,
                                // DIV-0027); every call of its clones re-aimed at a recorder, its two jump
                                // tables relocated in the copies
    TextMsgBoxFromMenuWindows_Inject();
    DisplaySetup_Inject();      // after GfxFilter, whose patch of the original set-up's bytes serves the BOF3X_ORIGINAL path
    WinMain_Inject();           // the window and the frame loop (DIV-0032..0034): no clones, order does not matter
    if (bof3::WantsShadow("area_backdrop")) area_backdrop::SelfTest();      // every call of its clones re-aimed at a recorder: order does not matter,
                                // except that it runs before Widescreen_Inject, so its fuzz compares the
                                // original's 320-wide backdrop quad
    Widescreen_Inject();        // DIV-0041, BOF3X_WIDE: after the modules above, so their fuzz ran against the original culls
    wide_rand::Inject();        // DIV-0041: the map walk's random split - after Widescreen_Inject, whose inset columns its
                                // log names; its self-test runs ours alone (DrawLayer_Open, stand-in handlers), so
                                // any later slot would do
    WorldMap_Inject();          // every call of its clones re-aimed at a recorder, its state table rebuilt in the
                                // copy; none of its functions goes through a cull, so after Widescreen is fine
    BattleDraw_Inject();        // every call of its clones re-aimed at a recorder, the device a fake: order
                                // does not matter (nothing before it patches bytes inside its six)
    BattleWindowDraw_Inject();  // every call of its clones re-aimed at a recorder and no byte of its bodies
                                // patched by any module: order does not matter (none of it reaches a cull)
    Battle3FromBattleWindowDraw_Inject();
    BattleWindows_Inject();     // every call of its clones re-aimed at a recorder, its state table, window-kind
                                // handlers and switch table moved in the copies: order does not matter
    if (bof3::WantsShadow("battle_flow")) battle_flow::SelfTest();        // round 7 group BB: every call of its clones re-aimed at a recorder, its two
                                // stack tables re-aimed and its jump table relocated in the copies: order
                                // does not matter (no widescreen patch touches its functions)
    Battle2_Inject();
    if (bof3::WantsShadow("battle_setup")) battle_setup::SelfTest();       // group BA (round 7): every call of its clones re-aimed at a recorder, no jump
                                // tables - order does not matter
    BattleMisc_Inject();        // every call of its clones re-aimed at a recorder, the dispatch's table
                                // immediates re-aimed and a jump table relocated in the copies: order does not matter
    if (bof3::WantsShadow("battle_damage")) battle_damage::SelfTest();      // every call of its clones re-aimed at a recorder, its jump table relocated
                                // and the effect handler tables swapped: order does not matter
    if (bof3::WantsShadow("battle_items")) battle_items::SelfTest();       // every call of its clones re-aimed at a recorder, Sparkle_Types' entries
                                // swapped and the stream's buffer a fake: order does not matter
    BattleSprites_Inject();     // group BG, round seven: every call of its clones re-aimed at a recorder,
                                // three jump tables relocated in the copies, the boss table swapped: order does not matter
    Battle5FromBattleSprites_Inject();
    Battle4FromBattleSprites_Inject();
    InventoryOps_Inject();      // every call of its clones re-aimed at a recorder: order does not matter
    if (bof3::WantsShadow("battle_odds")) battle_odds::SelfTest();        // group CK, round eight: every call and tail jmp of its clones re-aimed at a
                                // recorder: order does not matter
    if (bof3::WantsShadow("window_kinds")) window_kinds::SelfTest();       // round 8 group CM: every call of its clones re-aimed at a recorder and its three
                                // stack tables re-aimed in the copies: order does not matter (it clones its twelve
                                // before injecting them, and no module patches bytes inside them)
    if (bof3::WantsShadow("battle_fx_tasks")) battle_fx_tasks::SelfTest();     // group CE, round eight: every call of its clones re-aimed at a recorder, six stack
                                // tables re-aimed, a jump table relocated and Magic_Rows swapped: order does not matter
    BattleResult_Inject();      // round 8 group CD: every call of its clones re-aimed at a recorder, its two
                                // stack tables re-aimed in the copies and two .data step tables swapped:
                                // order does not matter
    MenuListFromBattleResult_Inject();
    BattleWinStates_Inject();   // group CL (round 8): every call of its clones re-aimed at a recorder and its
                                // seven stack tables' immediates re-aimed in the copies: order does not matter
    MenuListFromBattleWinStates_Inject();
    if (bof3::WantsShadow("enemy_ai_ops")) enemy_ai_ops::SelfTest();        // round 8 group CF: every call of its clones re-aimed at a recorder and its op
                                // tables' entries swapped: order does not matter (it clones before its own Inject)
    if (bof3::WantsShadow("battle_obj_states")) battle_obj_states::SelfTest();   // round 8 group CG: every call of its clones re-aimed at a recorder, its seven
                                // table operands aimed at tables of recorders in the copies: order does not matter
    if (bof3::WantsShadow("battle_menu_states")) battle_menu_states::SelfTest();  // round 8 group CI: every call of its clones re-aimed at a recorder and the four
                                // dispatch tables' entries swapped for recorders: order does not matter
    BattleActions_Inject();     // round 8 group CB: every call of its clones re-aimed at a recorder, the seven
                                // step tables' entries swapped in .data: order does not matter
    BattleActionFromBattleActions_Inject();
    if (bof3::WantsShadow("battle_actor_copies")) battle_actor_copies::SelfTest(); // round 8 group CH: every call of its clones re-aimed at a recorder and its five
                                // .data tables' entries swapped for recorders: order does not matter
    BattlePhases_Inject();      // round 8 group CA: every call of its clones re-aimed at a recorder, its two
                                // stack tables re-aimed in the copies and its three .data tables swapped for
                                // recorders: order does not matter (it clones only its own eighteen)
    BattleIntro_Inject();
    BattleTurnSteps_Inject();   // round 8 group CC: every call of its clones re-aimed at a recorder, two jump
                                // tables relocated and eight .data dispatch tables swapped for recorders:
                                // order does not matter
    BattleActionFromBattleTurnSteps_Inject();
    if (bof3::WantsShadow("magic_fx_reached")) magic_fx_reached::SelfTest();    // round 8 group CJ: every call of its clones re-aimed at a recorder, its five stack
                                // tables re-aimed in the copies and three .data tables swapped: order does not matter
    MagicFx3_Inject();
    FxDiscFan_Inject();
    MenuDrawHelpers_Inject();   // round 8 group DI: every call of its clones re-aimed at a recorder and its eleven
                                // .data dispatch tables swapped for recorders: order does not matter, except that it
                                // runs after Widescreen_Inject, whose bound inside 0x59B440 ours reads back
    ShopWin_Inject();
    Window2_Inject();
    MenuWin_Inject();
    MenuLists_Inject();         // round 8 group DH: every call of its clones re-aimed at a recorder and its eight
                                // .data dispatch tables' entries swapped for recorders: order does not matter
    MenuListFromMenuLists_Inject();
    ShopStates_Inject();        // round 8 group DF: every call of its clones re-aimed at a recorder and its five .data
                                // dispatch tables' entries swapped for recorders: order does not matter
    FieldSaveFromShopStates_Inject();
    ShopMode_Inject();
    if (bof3::WantsShadow("worldmap_area")) worldmap_area::SelfTest();      // round 8 group DA: every call and tail jmp of its clones re-aimed at a recorder
                                // and its seven .data dispatch tables swapped for recorders: order does not matter
                                // (it clones its thirty before injecting them; WorldMap_Inject cloned 0x404160 earlier)
    WorldMapHud_Inject();
    Area29_Inject();
    EffectWorldMapFromWorldmapArea_Inject();
    EffectKind34FromWorldmapArea_Inject();
    EffectKind06FromWorldmapArea_Inject();
    if (bof3::WantsShadow("field_hidden")) field_hidden::SelfTest();       // round 8 group DE: every call of its clones re-aimed at a recorder and its two
                                // .data tables' entries swapped for recorders: order does not matter
    Sprite_Inject();
    Encounter_Inject();
    Effect2_Inject();
    EventLeader_Inject();       // round 8 group DC: every call of its clones re-aimed at a recorder and its five
                                // .data dispatch tables' entries swapped for recorders: order does not matter
    if (bof3::WantsShadow("mode_states")) mode_states::SelfTest();        // round 8 group DB: every call of its clones re-aimed at a recorder, the system
                                // choice's stack table re-aimed, three jump tables relocated and four .data
                                // tables swapped for recorders: order does not matter
    TextMsgBoxFromModeStates_Inject();
    TransitionFromModeStates_Inject();
    Text_Inject();
    ShopStates2_Inject();       // round 8 group DG: every call of its clones re-aimed at a recorder and its two .data
                                // dispatch blocks swapped for recorders: order does not matter
    ShopTradeFromShopStates2_Inject();
    MapFieldObjects_Inject();   // round 8 group DD: every call of its clones re-aimed at a recorder and its jump
                                // table relocated in the copy; no module patches bytes inside its sixteen: order
                                // does not matter
    if (bof3::WantsShadow("task_sched")) task_sched::SelfTest();         // round 9 group EA: the scheduler unit 0x5A98A0..0x5A9A21 cloned whole (it calls
                                // nothing) and run on the fuzz's own stacks; no module patches bytes inside it:
                                // order does not matter (other clones' calls to Task_Sleep and the rest are
                                // re-aimed at their own recorders, and a call site keeps its target either way)
    Task_Inject();
    if (bof3::WantsShadow("magic_steal")) magic_steal::SelfTest();        // round 9 group SH (the spell harness): its clones' calls and stack-table immediates
                                // re-aimed at the shared harness's recorders; after Cheats_Inject, whose DIV-0046
                                // patch inside 0x4F5140 ours reads back and the copy carries: otherwise order
                                // does not matter
    SkillSteal_Inject();
    if (bof3::WantsShadow("magic_lib")) magic_lib::SelfTest();          // round 9 group L (the effect library): its clones' calls and the popup tasks'
                                // stack-table immediates re-aimed at the shared harness's recorders; no module
                                // patches bytes inside its 25: order does not matter
    MagicFx4_Inject();
    if (bof3::WantsShadow("magic_s16")) magic_s16::SelfTest();          // round 9 group S16 (MAGIC071..074): its clones' calls, stack-table immediates and
                                // four .data dispatch cells re-aimed at the harness's recorders; no module patches
                                // bytes inside its sixty (DIV-0046's masks are Pilfer's and Steal's): order does
                                // not matter
    if (bof3::WantsShadow("magic_s18")) magic_s18::SelfTest();          // round 9 group S18 (MAGIC079 / MAGIC082 through the spell harness): its clones' calls,
                                // stack-table immediates and eight .data tables re-aimed at recorders; no module
                                // patches bytes inside its 42: order does not matter
    DrainOrbA_Inject();
    BuffSpike_Inject();
    if (bof3::WantsShadow("magic_s21")) magic_s21::SelfTest();          // round 9 group S21 (MAGIC093..095, the spell harness): no module patches bytes
                                // inside its 48 (DIVERGENCE.md, cheats.cpp); its clones' calls, stack-table
                                // immediates and .data tables re-aimed at the harness's recorders: order does
                                // not matter
    Iceblast_Inject();
    FrostRing_Inject();
    if (bof3::WantsShadow("magic_s24")) magic_s24::SelfTest();          // round 9 group S24 (MAGIC104..106): no patch in its band; its calls into group L
                                // and the engine go through raw addresses re-aimed at the harness's recorders
    Fx106_Inject();
    Fx105_Inject();
    Fx104_Inject();
    if (bof3::WantsShadow("magic_s17")) magic_s17::SelfTest();          // round 9 group S17 (MAGIC075 / 077 / 078): its clones' calls, stack-table
                                // immediates and .data handler tables re-aimed at the shared harness's recorders;
                                // no module patches bytes inside its 48: order does not matter
    Revive_Inject();
    Purify_Inject();
    LeechShell_Inject();
    if (bof3::WantsShadow("magic_s19")) magic_s19::SelfTest();          // round 9 group S19 (MAGIC083, MAGIC086): its clones' calls, stack-table immediates
                                // and eight .data handler tables re-aimed at the shared harness's recorders; no
                                // module patches bytes inside its 43: order does not matter
    ShieldSpark_Inject();
    BarrierRing_Inject();
    BarrierLine_Inject();
    Barrier_Inject();
    if (bof3::WantsShadow("magic_s22")) magic_s22::SelfTest();          // round 9 group S22 (MAGIC096..099: Blizzard, Jolt, Lightning, Myollnir): its
                                // clones' calls, stack-table immediates and .data handler tables re-aimed at the
                                // shared harness's recorders, three through its own; no module patches bytes inside
                                // its 56: order does not matter
    Myollnir_Inject();
    LightningBolt_Inject();
    JoltBolt_Inject();
    BlizzardShard_Inject();
    if (bof3::WantsShadow("magic_s23")) magic_s23::SelfTest();          // round 9 group S23 (Cyclone, Typhoon, Quake, Simoon): its clones' calls re-aimed at
                                // the harness's recorders, or left on the GTE / GPU library both sides call; after
                                // every module it calls through (psx_gte*, psx_gpu, map_cells, world_map); no
                                // module patches bytes inside its 51: otherwise order does not matter
    SimoonDust_Inject();
    FxSpiral_Inject();
    FxFunnel_Inject();
    if (bof3::WantsShadow("magic_s25")) magic_s25::SelfTest();          // round 9 group S25 (MAGIC107..110): its clones' calls, stack-table immediates and
                                // the one jump table re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 56: order does not matter
    SpellSleep_Inject();
    SpellRagnarok_Inject();
    SpellDepress_Inject();
    SpellConfuse_Inject();
    MagicS20_Inject();          // round 9 group S20 (MAGIC087, 088, 092): its clones' calls, stack-table immediates
                                // and jump table re-aimed or moved, its thirteen .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 51: order does not matter
    Magic092_Inject();
    Magic088_Inject();
    Magic087_Inject();
    if (bof3::WantsShadow("magic_engine")) magic_engine::SelfTest();       // round 9 group E (the engine-side rows 0, 108, 123, 126, 128 and Head Cracker's
                                // rock): its clones' calls and stack-table immediates re-aimed at the shared
                                // harness's recorders; no module patches bytes inside its 22: order does not matter
    Paralyzer_Inject();
    MagicHold_Inject();
    HeadCrackerRock_Inject();
    HeadCracker_Inject();
    if (bof3::WantsShadow("magic_c3")) magic_c3::SelfTest();           // round 9 group C3 (MAGIC002, MAGIC111: the overlays no ability loads): its clones'
                                // calls and stack-table immediates re-aimed, its three .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 20: order does not matter
    Magic111_Inject();
    EffectKind5fFromMagicEngine_Inject();
                                // calls and stack-table immediates re-aimed, its three .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 20: order does not matter
    Magic002Ball_Inject();
    if (bof3::WantsShadow("magic_s29")) magic_s29::SelfTest();          // round 9 group S29 (MAGIC125, 126: DivineBreath, ShadowBreath): its clones' calls,
                                // stack-table immediates and eleven .data handler tables re-aimed at the shared
                                // harness's recorders, _ftol left to the copy; no module patches bytes inside its
                                // 49: order does not matter
    ShadowMote_Inject();
    if (bof3::WantsShadow("magic_s31")) magic_s31::SelfTest();          // round 9 group S31 (MAGIC132, 137, 138, 143): its clones' calls, stack-table
                                // immediates and jump table re-aimed or moved, its eight .data tables swapped for
                                // the fuzz only; no module patches bytes inside its 51: order does not matter
    ThunderClapBolt_Inject();
    DoomBreathOrb_Inject();
    CoronaRay_Inject();
    MainCannon_Inject();
    if (bof3::WantsShadow("magic_s28")) magic_s28::SelfTest();          // round 9 group S28 (MAGIC122..124: Firebreath, Icebreath, ThundrBreath, and
                                // Port_DroppedCall 0x4DF820): its clones' calls and stack-table immediates re-aimed
                                // at the shared harness's recorders, its seven .data tables swapped for the fuzz
                                // only; after every module whose fuzz clones a caller of 0x4DF820 (they re-aim that
                                // site themselves); DIV-0011 re-aims four calls to it, bytes outside it
    BreathMote_Inject();
    BreathBeam_Inject();
    if (bof3::WantsShadow("magic_s27")) magic_s27::SelfTest();          // round 9 group S27 (MAGIC118, 120, 121): its clones' calls, stack-table immediates
                                // and two jump tables re-aimed or moved, its six .data tables swapped for the fuzz
                                // only; no module patches bytes inside its 47: order does not matter
    WhelpBreathFlames_Inject();
    Burn_Inject();
    DragonBreathBeam_Inject();
    if (bof3::WantsShadow("magic_s26")) magic_s26::SelfTest();          // round 9 group S26 (MAGIC114, 115, 117): its clones' calls, stack-table immediates,
                                // jump tables and seven .data tables re-aimed or moved for the fuzz only; no module
                                // patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp): order does not matter
    Magic117_Inject();
    Magic115_Inject();
    Magic114_Inject();
    if (bof3::WantsShadow("magic_s30")) magic_s30::SelfTest();          // round 9 group S30 (MAGIC130, MAGIC131): its clones' calls, stack-table immediates and
                                // eleven .data tables re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 60 (DIVERGENCE.md, cheats.cpp): order does not matter
    VenomRing_Inject();
    MagicFx2_Inject();
    MagicFx_Inject();
    KaiserRing_Inject();
    KaiserPillar_Inject();
    Kaiser_Inject();
    if (bof3::WantsShadow("magic_c2")) magic_c2::SelfTest();           // round 9 group C2 (MAGIC057, 081, 116, 129: Bone Dance, RottenBreath, UtmostAttack,
                                // Holocaust): its clones' calls, stack-table immediates and .data handler tables
                                // re-aimed at the shared harness's recorders; no module patches bytes inside its 64:
                                // order does not matter
    HolocaustBeam_Inject();
    UtmostAttack_Inject();
    RottenBreathMote_Inject();
    BoneDance_Inject();
    if (bof3::WantsShadow("magic_c1")) magic_c1::SelfTest();           // round 9 group C1 (MAGIC010, 080, 113, 145, 146, 213: the unfinished skills): its
                                // clones' calls, stack-table immediates and fifteen .data tables re-aimed at the
                                // shared harness's recorders; no module patches bytes inside its 62: order does
                                // not matter
    Magic213Mote_Inject();
    Pentagram_Inject();
    WhiteFlag_Inject();
    if (bof3::WantsShadow("magic_s01")) magic_s01::SelfTest();          // round 9 group S01 (MAGIC001: rows 1 and 105, Nue Stomp and Jump): its clones'
                                // calls and stack-table immediates re-aimed at the shared harness's recorders, its
                                // one .data table swapped for the fuzz only; no module patches bytes inside its 26
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    NueStompChild_Inject();
    JumpChild_Inject();
    if (bof3::WantsShadow("magic_s06")) magic_s06::SelfTest();          // round 9 group S06 (MAGIC008, MAGIC020): its clones' calls, stack-table immediates
                                // and two .data tables re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 56 (DIVERGENCE.md, cheats.cpp): order does not matter
    Magic020Lead_Inject();
    Magic008Grow_Inject();
    Magic008Dash_Inject();
    Magic008_Inject();
    if (bof3::WantsShadow("magic_s05")) magic_s05::SelfTest();          // round 9 group S05 (MAGIC017, MAGIC018/019): its clones' calls, stack-table
                                // immediates and one .data table re-aimed at the shared harness's recorders; no
                                // module patches bytes inside its 29 (DIVERGENCE.md, cheats.cpp): order does not
                                // matter
    Magic019_Inject();
    Magic018Row54_Inject();
    Magic018Row53_Inject();
    Magic018Row5_Inject();
    Magic017Image_Inject();
    MagicS02_Inject();          // round 9 group S02 (MAGIC003: Super Combo; MAGIC004, the code of ten Strike and Claw
                                // files): its clones' calls, stack-table immediates and jump table re-aimed or moved,
                                // its four .data tables swapped for the fuzz only; no module patches bytes inside its
                                // 48 (DIVERGENCE.md, cheats.cpp): order does not matter
    SuperCombo_Inject();
    ElemStrike_Inject();
    if (bof3::WantsShadow("magic_s03")) magic_s03::SelfTest();          // round 9 group S03 (MAGIC006, 009, 012: Mind Sword, Chlorine, Blitz): its clones'
                                // calls and stack-table immediates re-aimed at the shared harness's recorders, its
                                // five .data tables swapped for the fuzz only; no module patches bytes inside its
                                // 44 (DIVERGENCE.md, cheats.cpp): order does not matter
    MindSwordBlade_Inject();
    Chlorine_Inject();
    BlitzBolt_Inject();
    if (bof3::WantsShadow("magic_s08")) magic_s08::SelfTest();          // round 9 group S08 (MAGIC041, 042, 043, 044): its clones' calls, stack-table
                                // immediates and seven .data tables re-aimed at the shared harness's recorders; no
                                // module patches bytes inside its 59 (DIVERGENCE.md, cheats.cpp): order does not matter
    WardMote_Inject();
    EvilEyeBeam_Inject();
    Berserk_Inject();
    CounterMark_Inject();
    if (bof3::WantsShadow("magic_s04")) magic_s04::SelfTest();          // round 9 group S04 (MAGIC013, MAGIC015 with 016 folded: Snap, Charge, Flying Kick,
                                // Air Raid): its clones' calls, stack-table immediates, two jump tables and five
                                // .data tables re-aimed or moved for the fuzz only; no module patches bytes inside
                                // its 56 (DIVERGENCE.md, cheats.cpp): order does not matter
    SnapWave_Inject();
    KickImage_Inject();
    if (bof3::WantsShadow("magic_s07")) magic_s07::SelfTest();          // round 9 group S07 (MAGIC021, 038, 039, 040: Bonebreak, War Shout, Focus,
                                // Enlighten): its clones' calls, stack-table immediates and eight .data tables
                                // re-aimed at the shared harness's recorders; no module patches bytes inside its 59
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    WarShoutMote_Inject();
    FocusAura_Inject();
    EnlightenRays_Inject();
    BonebreakMote_Inject();
    if (bof3::WantsShadow("magic_s13")) magic_s13::SelfTest();          // round 9 group S13 (MAGIC063: Sudden Death): its clones' calls and stack-table
                                // immediates re-aimed at the shared harness's recorders, its four .data tables
                                // swapped for the fuzz only; no module patches bytes inside its 25 (DIVERGENCE.md,
    SuddenDeathChild_Inject();
    if (bof3::WantsShadow("magic_s11")) magic_s11::SelfTest();          // round 9 group S11 (MAGIC058, 059: Sanctuary, Tornado): its clones' calls,
                                // stack-table immediates and three .data table runs re-aimed at the shared
                                // harness's recorders; no module patches bytes inside its 35 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    TornadoMote_Inject();
    SanctuaryMote_Inject();
    if (bof3::WantsShadow("magic_s09")) magic_s09::SelfTest();          // round 9 group S09 (MAGIC045, 046/047, 048, 050: Bone Dart, Firebreath / Icebreath,
                                // Dream Breath, Pollen / Venom Breath): its clones' calls, stack-table immediates and
                                // eight .data tables re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 47 (DIVERGENCE.md, cheats.cpp): order does not matter
    PollenMote_Inject();
    ElemBreathMote_Inject();
    DreamBreathMote_Inject();
    BoneDartShaft_Inject();
    if (bof3::WantsShadow("magic_s10")) magic_s10::SelfTest();          // round 9 group S10 (MAGIC052..056: Ovum, Lavaburst, Howling, Ebonfire,
                                // Sacrifice): its clones' calls, stack-table immediates and nine .data tables
                                // re-aimed at the shared harness's recorders; no module patches bytes inside its 60
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    SacrificeActor_Inject();
    Ovum_Inject();
    LavaburstChild_Inject();
    HowlingChild_Inject();
    EbonfireRing_Inject();
    Ebonfire_Inject();
    if (bof3::WantsShadow("magic_s12")) magic_s12::SelfTest();          // round 9 group S12 (MAGIC060, MAGIC062: Identify, Celerity): its clones' calls,
                                // stack-table immediates, one jump table and seven .data tables re-aimed or moved
                                // for the fuzz only; no module patches bytes inside its 44 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Identify_Inject();
    CeleritySpark_Inject();
    if (bof3::WantsShadow("magic_s14")) magic_s14::SelfTest();          // round 9 group S14 (MAGIC064, 066 and MAGIC065's last: Weretiger, Pilfer,
                                // Tsunami): its clones' calls, stack-table immediates and four .data tables re-aimed
                                // at the shared harness's recorders; no module patches bytes inside its 57 (DIVERGENCE.md,
                                // cheats.cpp: DIV-0046 patches Pilfer's 0x4B5691, not ours): order does not matter
    Weretiger_Inject();
    TsunamiWave_Inject();
    Steal_Inject();
    if (bof3::WantsShadow("magic_s15")) magic_s15::SelfTest();          // round 9 group S15 (MAGIC067, 068, 069: Chill, Foretell, Influence): its clones'
                                // calls, stack-table immediates and jump table re-aimed or moved, its ten .data
                                // tables swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Sparkle_Inject();
    InfluenceRight_Inject();
    Foretell_Inject();
    ChillRay_Inject();
    if (bof3::WantsShadow("magic_s32")) magic_s32::SelfTest();          // round 9 group S32 (MAGIC144, 150: Wall of Fire, Eye Beam): its clones' calls and
                                // stack-table immediates re-aimed at the shared harness's recorders, its five .data
                                // tables swapped for the fuzz only; no module patches bytes inside its 36
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    WallOfFireChild_Inject();
    EyeBeam_Inject();
    if (bof3::WantsShadow("magic_s33")) magic_s33::SelfTest();          // round 9 group S33 (MAGIC151, 154: Accession, Mighty Chop): its clones' calls
                                // and stack-table immediates re-aimed, its seven .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 57 (DIVERGENCE.md, cheats.cpp)
    MightyChopBlade_Inject();
    AccessionSpark_Inject();
    AccessionRing_Inject();
    AccessionOrb_Inject();
    AccessionCtl_Inject();
    AccessionBolt_Inject();
    Accession_Inject();
    if (bof3::WantsShadow("magic_s34")) magic_s34::SelfTest();          // round 9 group S34 (MAGIC158, 159, 161, 162, 166: Charm, (no label), Timed Blow,
                                // Transfer, Monopolize): its clones' calls and stack-table immediates re-aimed at the
                                // shared harness's recorders, its eight .data tables swapped for the fuzz only; no
                                // module patches bytes inside its 47 (DIVERGENCE.md, cheats.cpp): order does not matter
    TransferMote_Inject();
    TimedBlow_Inject();
    MonopolizeMote_Inject();
    Magic159Child_Inject();
    CharmMote_Inject();
    if (bof3::WantsShadow("magic_s36")) magic_s36::SelfTest();          // round 9 group S36 (MAGIC172, 173, 218: Magic Ball, Intimidate, Aura Breath): its
                                // clones' calls and stack-table immediates re-aimed, its eight .data tables swapped
                                // for the fuzz only; no module patches bytes inside its 45 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    MagicBall_Inject();
    IntimidateTrail_Inject();
    AuraBreathDome_Inject();
    if (bof3::WantsShadow("magic_s37")) magic_s37::SelfTest();          // round 9 group S37 (MAGIC219, 220/221, 222: Magma Breath, Geo / Gaea's Breath,
                                // Combustion): its clones' calls and stack-table immediates re-aimed at the shared
                                // harness's recorders, its eleven .data tables swapped for the fuzz only; no module
                                // patches bytes inside its 60 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagmaBreathRecord_Inject();
    BattleFx2_Inject();
    CombustionFlash_Inject();
    CombustionMote_Inject();
    CombustionGlow_Inject();
    GeoBreathChild_Inject();
    Combustion_Inject();
    if (bof3::WantsShadow("magic_s35")) magic_s35::SelfTest();          // round 9 group S35 (MAGIC167, 168, 169: Last Resort, Cure, Benediction): its
                                // clones' calls and stack-table immediates re-aimed at the shared harness's recorders,
                                // its six .data tables swapped for the fuzz only; no module patches bytes inside its 46
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    LastResortBeam_Inject();
    CureMote_Inject();
    BenedictionChild_Inject();
    if (bof3::WantsShadow("magic_s38")) magic_s38::SelfTest();          // round 9 group S38 (MAGIC223, 225, 226/227: Tempest / Hurricane, an unlabelled id,
                                // MeteorStrike): its clones' calls and stack-table immediates re-aimed at the shared
                                // harness's recorders, its thirteen .data tables swapped for the fuzz only; no module
                                // patches bytes inside its 54 (DIVERGENCE.md, cheats.cpp): order does not matter
    MeteorStrikeRock_Inject();
    Magic225Veil_Inject();
    if (bof3::WantsShadow("area_011")) area_011::SelfTest();           // round 10 group ARH (area 11: two handlers and the init, the area harness's proof):
                                // its clones' calls re-aimed at the area harness's recorders; no module patches
                                // bytes inside its 3 (DIVERGENCE.md, cheats.cpp): order does not matter
    Area11_Inject();
    if (bof3::WantsShadow("area_cell_hook")) area_cell_hook::SelfTest();      // round 10 group ARH (0x56E670, the cell hook's per-area reader): its table's
                                // handlers swapped for the fuzz only; nothing patches inside it
    if (bof3::WantsShadow("area_w0b")) area_w0b::SelfTest();           // round 10 group AR0B (world 0's areas 16 and 18..26): its clones' calls re-aimed at
                                // the area harness's recorders, area 16's six .data state tables swapped for the fuzz
                                // only; no module patches bytes inside its 61 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    Area26_Inject();
    Area23_Inject();
    Area22_Inject();
    Area21_Inject();
    Area20_Inject();
    Area19_Inject();
    Area18_Inject();
    Area16_Inject();
    if (bof3::WantsShadow("scena_sc0")) scena_sc0::SelfTest();          // round 10 group SCH (scenario chapter 0, the scenario harness's proof): its clones'
                                // calls re-aimed at the scenario harness's recorders, its three .data tables swapped
                                // for the fuzz only; no module patches bytes inside its 19 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Scena00_Inject();
    if (bof3::WantsShadow("scena_se")) scena_se::SelfTest();           // round 10 group SE (the chapters' shared engine-side helpers, six): its clones'
                                // calls re-aimed at the scenario harness's recorders; no module patches bytes
                                // inside its 6 (DIVERGENCE.md, cheats.cpp): order does not matter
    Field_Inject();
    if (bof3::WantsShadow("scena_sc11")) scena_sc11::SelfTest();         // round 10 group SC11 (scenario chapter 11): its clones' calls re-aimed at the scenario
                                // harness's recorders, its three .data tables swapped for the fuzz only; no module
                                // patches bytes inside its 30 (DIVERGENCE.md, cheats.cpp): order does not matter
    Scena11_Inject();
    if (bof3::WantsShadow("scena_sc3")) scena_sc3::SelfTest();          // round 10 group SC3 (scenario chapters 3 and 4): its clones' calls re-aimed at the
                                // scenario harness's recorders, its tables swapped for the fuzz only; no module
                                // patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp): order does not matter
    Scena042_Inject();
    Scena03_Inject();
    if (bof3::WantsShadow("scena_sc1")) scena_sc1::SelfTest();          // round 10 group SC1 (scenario chapter 1's bank): its clones' calls re-aimed at the
                                // scenario harness's recorders, its three .data tables swapped for the fuzz only;
                                // no module patches bytes inside its 42: order does not matter
    Scena012_Inject();
    if (bof3::WantsShadow("scena_sc12")) scena_sc12::SelfTest();         // round 10 group SC12 (scenario chapter 12's first block, 0x55E4E0..0x561DB0):
                                // no module patches bytes inside its 24 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    Scena122_Inject();
    ScenaCalls_Inject();        // round 10 group CALLS (the chapter call tables' 98 entries, 0x519890..0x51AC50):
                                // its clones' calls re-aimed at the scenario harness's recorders; no module patches
                                // bytes inside its 98 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaCall5_Inject();
    ScenaCall4_Inject();
    ScenaCall3_Inject();
    ScenaCall2_Inject();
    ScenaCall_Inject();
    Scena15_Inject();
    Scena14_Inject();
    Scena13_Inject();
    Scena12_Inject();
    Scena102_Inject();
    Scena10_Inject();
    Scena09_Inject();
    Scena08_Inject();
    Scena07_Inject();
    Scena062_Inject();
    Scena06_Inject();
    Scena05_Inject();
    Scena04_Inject();
    Scena02_Inject();
    Scena01_Inject();
    ScenaSc9a_Inject();         // round 10 group SC9a (scenario chapter 9's first block, 0x553B30..0x557170):
                                // no module patches bytes inside its 22 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    if (bof3::WantsShadow("area_w0a")) area_w0a::SelfTest();           // round 10 group AR0A (world 0's areas 0..5, 7, 8, 10, 12, 13, 15): its clones' calls
                                // re-aimed at the area harness's recorders, area 15's two state tables swapped for
                                // the fuzz only; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp):
    Area15_Inject();
    Area13_Inject();
    Area12_Inject();
    Area10_Inject();
    Area08_Inject();
    Area07_Inject();
    Area052_Inject();
    Area05_Inject();
    Area03_Inject();
    Area02_Inject();
    Area01_Inject();
    Area00_Inject();
    if (bof3::WantsShadow("scena_sc6")) scena_sc6::SelfTest();          // round 10 group SC6 (scenario chapter 6's bank, 0x54A910..0x54F080): its clones'
                                // calls re-aimed at the scenario harness's recorders, its tables swapped for the
                                // fuzz only; no module patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Scena063_Inject();
    if (bof3::WantsShadow("area_w0c")) area_w0c::SelfTest();           // round 10 group AR0C (world 0, areas 27..29 and 32..37): its clones' calls
                                // re-aimed at the area harness's recorders, its five .data state tables swapped
                                // for the fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area37_Inject();
    Area35_Inject();
    Area34_Inject();
    Area32_Inject();
    Area28_Inject();
    Area27_Inject();
    if (bof3::WantsShadow("scena_sc7")) scena_sc7::SelfTest();          // round 10 group SC7 (scenario chapters 7 and 8, 0x54F080..0x553B30): its clones'
                                // calls re-aimed at the scenario harness's recorders, its tables swapped for the
                                // fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Scena082_Inject();
    Scena072_Inject();
    if (bof3::WantsShadow("scena_sc5")) scena_sc5::SelfTest();          // round 10 group SC5 (scenario chapter 5's bank, 0x546390..0x54A910): no module
                                // patches bytes inside its 35 (DIVERGENCE.md, cheats.cpp): order does not matter
    Scena052_Inject();
    if (bof3::WantsShadow("scena_sx")) scena_sx::SelfTest();           // round 10 group SX (the engine callees the chapters and areas call by raw address,
                                // eighteen): its clones' calls re-aimed at the scenario harness's recorders; no
                                // module patches bytes inside its 18 (DIVERGENCE.md, cheats.cpp): order does not
    FieldChar_Inject();
    if (bof3::WantsShadow("area_w1b")) area_w1b::SelfTest();           // round 10 group AR1B (world 1, areas 42..47): its clones' calls re-aimed at the
                                // area harness's recorders, its .data state tables swapped for the fuzz only; no
                                // module patches bytes inside its 55 (DIVERGENCE.md, cheats.cpp): order does not
                                // matter
    Area47_Inject();
    Area46_Inject();
    Area45_Inject();
    Area43_Inject();
    Area42_Inject();
    ScenaSc9b_Inject();         // round 10 group SC9b (scenario chapter 9's tail and chapter 10, 0x557170..0x55C040):
                                // its clones' calls re-aimed at the scenario harness's recorders, its tables swapped
                                // for the fuzz only; no module patches bytes inside its 63 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Scena103_Inject();
    Scena092_Inject();
    AreaW1c_Inject();           // round 10 group AR1C (world 1, areas 48..52, 0x408FF0..0x40AB00): its clones' calls
                                // re-aimed at the area harness's recorders; no module patches bytes inside its 57
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Area52_Inject();
    Area51_Inject();
    Area50_Inject();
    Area49_Inject();
    Area48_Inject();
    ScenaSc15_Inject();         // round 10 group SC15 (scenario chapter 15, 0x567DC0..0x56B2A0 and 0x537580; chapter 16's
                                // slot 1 0x56C080; chapters 17..19, 0x56C130..0x56D5E0): its clones' calls re-aimed at
                                // the scenario harness's recorders, its tables swapped for the fuzz only; no module
                                // patches bytes inside its 89 (DIVERGENCE.md, cheats.cpp): order does not matter
    Scena19_Inject();
    Scena18_Inject();
    Scena17_Inject();
    Scena162_Inject();
    Scena152_Inject();
    if (bof3::WantsShadow("area_w1a")) area_w1a::SelfTest();           // round 10 group AR1A (world 1, areas 38..41, 0x4053B0..0x406650): its clones' calls
                                // re-aimed at the area harness's recorders, areas 39 and 41's state tables swapped
                                // for the fuzz only; no module patches bytes inside its 48 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area41_Inject();
    Area40_Inject();
    Area39_Inject();
    if (bof3::WantsShadow("scena_sc2")) scena_sc2::SelfTest();          // round 10 group SC2 (scenario chapter 2's bank, 0x53DDA0..0x5428C0): no module
                                // patches bytes inside its 72 (DIVERGENCE.md, cheats.cpp): order does not matter
    Scena022_Inject();
    if (bof3::WantsShadow("scena_sx2")) scena_sx2::SelfTest();          // round 10 group SX2 (the engine callees nobody owned after SX, thirteen): its
                                // clones' calls re-aimed at the scenario harness's recorders; no module patches
                                // bytes inside its 13 (DIVERGENCE.md, cheats.cpp): order does not matter
    EffectKind06FromScenaSx2_Inject();
    if (bof3::WantsShadow("area_w1e")) area_w1e::SelfTest();           // round 10 group AR1E (world 1, areas 65 and 67, 0x40B8C0..0x40CEF0): its clones'
                                // calls re-aimed at the area harness's recorders, area 65's state tables swapped for
                                // the fuzz only; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area67_Inject();
    if (bof3::WantsShadow("area_w2a")) area_w2a::SelfTest();           // round 10 group AR2A (world 2, areas 76..82 and 84, 0x40EB90..0x40F720): its clones'
                                // calls re-aimed at the area harness's recorders, area 79's state table swapped for
                                // the fuzz only; no module patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area84_Inject();
    Area82_Inject();
    Area81_Inject();
    Area80_Inject();
    Area79_Inject();
    Area78_Inject();
    Area77_Inject();
    if (bof3::WantsShadow("area_w2b")) area_w2b::SelfTest();           // round 10 group AR2B (world 2, areas 85..88, 0x40F720..0x411F10): its clones' calls
                                // re-aimed at the area harness's recorders, the world maps' state tables swapped for
                                // the fuzz only; no module patches bytes inside its 66 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area87_Inject();
    Area86_Inject();
    Area85_Inject();
    if (bof3::WantsShadow("area_w1d")) area_w1d::SelfTest();           // round 10 group AR1D (world 1, areas 53, 55..57, 59..64, 0x40AB00..0x40B8C0): its
                                // clones' calls re-aimed at the area harness's recorders, areas 56 and 59's state
                                // tables and areas 63 / 64's weights swapped for the fuzz only; no module patches
                                // bytes inside its 47 (DIVERGENCE.md, cheats.cpp): order does not matter
    Area65_Inject();
    Area63_Inject();
    Area62_Inject();
    Area61_Inject();
    Area59_Inject();
    Area57_Inject();
    Area56_Inject();
    Area55_Inject();
    if (bof3::WantsShadow("area_w2d")) area_w2d::SelfTest();           // round 10 group AR2D (world 2, areas 95..100 and 103, 0x4135B0..0x4146C0): its
                                // clones' calls re-aimed at the area harness's recorders, areas 99 and 100's state
                                // tables swapped for the fuzz only; no module patches bytes inside its 53
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Area99_Inject();
    Area98_Inject();
    Area97_Inject();
    Area96_Inject();
    Area95_Inject();
    Area103_Inject();
    Area100_Inject();
    if (bof3::WantsShadow("area_w2c")) area_w2c::SelfTest();           // round 10 group AR2C (world 2, areas 90..92 and 94, 0x411F10..0x4135B0): its clones'
                                // calls re-aimed at the area harness's recorders, area 91's state table swapped for
                                // the fuzz only; no module patches bytes inside its 45 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area94_Inject();
    Area92_Inject();
    Area91_Inject();
    if (bof3::WantsShadow("scena_sc13")) scena_sc13::SelfTest();         // round 10 group SC13 (scenario chapters 13 and 14, 0x561DB0..0x567DC0): no module
                                // patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp): order does not matter
    Scena142_Inject();
    Scena132_Inject();
    if (bof3::WantsShadow("area_w3a")) area_w3a::SelfTest();           // round 10 group AR3A (world 3, areas 115..119, 0x418BE0..0x41A9D0): its clones' calls
                                // re-aimed at the area harness's recorders, area 115's and 116's .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 56 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area119_Inject();
    Area118_Inject();
    Area117_Inject();
    Area1152_Inject();
    AreaW3c_Inject();           // round 10 group AR3C (world 3, areas 124..125, 127..128, 130..134, 0x41C890..0x41DAD0):
                                // its clones' calls re-aimed at the area harness's recorders; no module patches bytes
                                // inside its 56 (DIVERGENCE.md, cheats.cpp): order does not matter
    Area133_Inject();
    Area132_Inject();
    Area131_Inject();
    Area130_Inject();
    Area128_Inject();
    Area127_Inject();
    Area125_Inject();
    Area124_Inject();
    if (bof3::WantsShadow("area_w3b")) area_w3b::SelfTest();           // round 10 group AR3B (world 3, areas 120..121, 0x41A9D0..0x41C890): its clones'
                                // calls re-aimed at the area harness's recorders, area 121's .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 54 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area121_Inject();
    Area120_Inject();
    if (bof3::WantsShadow("area_w2e")) area_w2e::SelfTest();           // round 10 group AR2E (world 2, areas 104..106, 0x4146C0..0x4168E0): its clones'
                                // calls re-aimed at the area harness's recorders, area 104's six .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area106_Inject();
    Area105_Inject();
    Area104_Inject();
    if (bof3::WantsShadow("area_w3f")) area_w3f::SelfTest();           // round 10 group AR3F (world 3, areas 143..146, 0x420800..0x4223A0): its clones'
                                // calls re-aimed at the area harness's recorders, its .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 53 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area146_Inject();
    Area145_Inject();
    Area144_Inject();
    Area143_Inject();
    if (bof3::WantsShadow("area_w3e")) area_w3e::SelfTest();           // round 10 group AR3E (world 3, areas 136 and 139..142, 0x41EFE0..0x420800): its
                                // clones' calls re-aimed at the area harness's recorders, the state tables of areas
                                // 140..142 swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Area142_Inject();
    Area141_Inject();
    Area140_Inject();
    Area139_Inject();
    Area136_Inject();
    AreaW3d_Inject();           // round 10 group AR3D (world 3, area 135, 0x41DAD0..0x41EFE0): its clones' calls
                                // re-aimed at the area harness's recorders, its four .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 46 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area135_Inject();
    Area134_Inject();
    if (bof3::WantsShadow("area_w2f")) area_w2f::SelfTest();           // round 10 group AR2F (world 2, areas 108 and 110..113, 0x4168E0..0x418BE0): its
                                // clones' calls re-aimed at the area harness's recorders, areas 108 and 112's state
                                // tables swapped for the fuzz only; no module patches bytes inside its 53
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Area115_Inject();
    Area112_Inject();
    Area111_Inject();
    Area108_Inject();
    AreaW1f_Inject();           // round 10 group AR1F (world 1, areas 68..69 and 71..75, 0x40CEF0..0x40EB90): its clones'
                                // calls re-aimed at the area harness's recorders, its .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 59 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area75_Inject();
    Area74_Inject();
    Area73_Inject();
    Area72_Inject();
    Area71_Inject();
    Area69_Inject();
    Area68_Inject();
    if (bof3::WantsShadow("area_w3g")) area_w3g::SelfTest();           // round 10 group AR3G (world 3, areas 148..151, 0x4223A0..0x4249D0): its clones'
                                // calls re-aimed at the area harness's recorders, its .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 56 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    Area151_Inject();
    Area149_Inject();
    Area148_Inject();
    AreaW4a_Inject();           // round 10 group AR4A (world 4, areas 152..155 and 166..167, 0x4249D0..0x426560): its
                                // clones' calls re-aimed at the area harness's recorders, area 152's six .data state
                                // tables swapped for the fuzz only; no module patches bytes inside its 48
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Area167_Inject();
    Area155_Inject();
    Area154_Inject();
    Area153_Inject();
    Area152_Inject();
    if (bof3::WantsShadow("area_w4c")) area_w4c::SelfTest();           // round 10 group AR4C (world 4, areas 173..174 and six choices of areas 175..185,
                                // 0x428450..0x4292C0): its clones' calls re-aimed at the area harness's recorders; no
                                // module patches bytes inside its 39 (DIVERGENCE.md, cheats.cpp): order does not matter
    Area174_Inject();
    Area173_Inject();
    if (bof3::WantsShadow("area_w4d")) area_w4d::SelfTest();           // round 10 group AR4D (world 4, areas 175..187, 0x4292C0..0x42A320): its clones'
                                // calls re-aimed at the area harness's recorders, area 175's glide states swapped
                                // for the fuzz only; no module patches bytes inside its 49 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area187_Inject();
    Area186_Inject();
    Area185_Inject();
    Area184_Inject();
    Area183_Inject();
    Area182_Inject();
    Area181_Inject();
    Area180_Inject();
    Area179_Inject();
    Area178_Inject();
    Area177_Inject();
    Area176_Inject();
    Area175_Inject();
    if (bof3::WantsShadow("area_w4b")) area_w4b::SelfTest();           // round 10 group AR4B (world 4, areas 168..172, 0x426560..0x428450): its clones'
                                // calls re-aimed at the area harness's recorders, area 172's two .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 56 (DIVERGENCE.md,
    Area172_Inject();
    Area171_Inject();
    Area170_Inject();
    Area169_Inject();
    Area168_Inject();
    if (bof3::WantsShadow("area_w4e")) area_w4e::SelfTest();           // round 10 group AR4E (world 4, areas 188..191, 0x42A320..0x42BD60): its clones'
                                // calls re-aimed at the area harness's recorders, areas 189 and 191's state tables
                                // swapped for the fuzz only; no module patches bytes inside its 51 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    Area191_Inject();
    Area189_Inject();
    Area188_Inject();
    if (bof3::WantsShadow("area_w4f")) area_w4f::SelfTest();           // round 10 group AR4F (world 4, areas 192..193 and 196..199, 0x42BD60..0x42D710):
                                // its clones' calls re-aimed at the area harness's recorders, areas 197 and 198's
                                // state tables swapped for the fuzz only; no module patches bytes inside its 49
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    Area199_Inject();
    Area198_Inject();
    Area197_Inject();
    Area196_Inject();
    Area193_Inject();
    Area192_Inject();
    if (bof3::WantsShadow("boss_spawn")) boss_spawn::SelfTest();         // round 11 group BH (the boss set-ups' spawn helpers, 0x4948E0..0x494A7D): its
                                // clones' calls re-aimed at the boss harness's recorders; no module patches bytes
                                // inside its 6 (DIVERGENCE.md, cheats.cpp): order does not matter
    Battle4FromBossSpawn_Inject();
    if (bof3::WantsShadow("boss_h")) boss_h::SelfTest();             // round 11 group BH (the boss band's 20 shared helpers, 0x437CA0..0x440829): its
                                // clones' calls re-aimed at the boss harness's recorders, kinds 8..11's shared
                                // tables swapped and six kinds' dispatchers driven with a clone planted in their
                                // tables for the fuzz only; no module patches bytes inside its 20 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    BareRet_Inject();
    if (bof3::WantsShadow("boss_sd")) boss_sd::SelfTest();            // round 11 group BSD (fights 17..21 and kinds 18, 21..27, 0x43A590..0x43B74A): its
                                // clones' calls re-aimed at the boss harness's recorders, its kinds' step, action
                                // and hook tables swapped for the fuzz only; no module patches bytes inside its 52
    if (bof3::WantsShadow("boss_sb")) boss_sb::SelfTest();            // round 11 group BSB (fights 4..10 and 13, kinds 3, 4, 5 and 8..11, 0x438290..0x43A022):
                                // its clones' calls re-aimed at the boss harness's recorders, its kinds' state and
                                // hook tables swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    BossWorker_Inject();
    BossTorast_Inject();
    BossOperator_Inject();
    BossKassen_Inject();
    BossGaltel_Inject();
    BossEngineer_Inject();
    BossDoksen_Inject();
    Boss07_Inject();
    Boss04_Inject();
    if (bof3::WantsShadow("boss_sa")) boss_sa::SelfTest();            // round 11 group BSA (fights 1, 2, 3, 39 and kinds 1, 2, 6, 7, 39, 46,
                                // 0x437A10..0x43D662): its clones' calls re-aimed at the boss harness's recorders,
                                // the six kinds' state and hook tables swapped for the fuzz only; no module patches
                                // bytes inside its 49 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossNue_Inject();
    BossGary_Inject();
    Boss02_Inject();
    Boss01_Inject();
    if (bof3::WantsShadow("boss_sc")) boss_sc::SelfTest();            // round 11 group BSC (fights 11, 12, 14, 15, 16, 46 and kinds 12..17, 53,
                                // 0x439410..0x43A589): its clones' calls re-aimed at the boss harness's recorders,
                                // seven kinds' tables swapped for the fuzz only; no module patches bytes inside
                                // its 53 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSunder_Inject();
    BossBalio_Inject();
    BossAmalgam_Inject();
    Boss08_Inject();
    if (bof3::WantsShadow("boss_se")) boss_se::SelfTest();            // round 11 group BSE (fights 22..26, 30, 48 and kinds 28..32, 55, 0x43B5B0..0x43E7A0):
                                // its clones' calls re-aimed at the boss harness's recorders, the kinds' state and
                                // hook tables swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    BossZig_Inject();
    BossStallion_Inject();
    BossNina_Inject();
    BossBully_Inject();
    BossBeyd_Inject();
    Boss24_Inject();
    Boss23_Inject();
    if (bof3::WantsShadow("boss_si")) boss_si::SelfTest();            // round 11 group BSI (fights 37, 38, 40, 42, 44, 45, 49..51, 53, kinds 45, 47, 49,
                                // 51, 52, 56..58, 60 and Arwan's task, 0x43ECC0..0x43F79B): its clones' calls
                                // re-aimed at the boss harness's recorders, the kinds' state and hook tables swapped
                                // for the fuzz only; no module patches bytes inside its 50 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    BossSample4_Inject();
    BossSample2_Inject();
    BossChimera_Inject();
    BossArwanFx_Inject();
    BossArwan_Inject();
    BossAmmonite_Inject();
    Boss53_Inject();
    Boss38_Inject();
    if (bof3::WantsShadow("boss_sh")) boss_sh::SelfTest();            // round 11 group BSH (fights 34..36, 41, 43, 47, kinds 41..44, 48, 50, 54 and the
                                // kind-3 slot 3, 0x43DEF0..0x43ECBF): its clones' calls re-aimed at the boss
                                // harness's recorders, the kinds' state and hook tables swapped for the fuzz only;
                                // no module patches bytes inside its 46 (DIVERGENCE.md, cheats.cpp): order does not
                                // matter
    BossGaist_Inject();
    BossElder_Inject();
    BossAnglerFx_Inject();
    BossAngler_Inject();
    Boss36_Inject();
    Boss35_Inject();
    if (bof3::WantsShadow("boss_sj")) boss_sj::SelfTest();            // round 11 group BSJ (fights 52, 54, 55, kinds 59, 61, effect slots 4 and 5,
                                // 0x43F7A0..0x441087): its clones' calls re-aimed at the boss harness's recorders,
                                // the kinds' and tasks' tables swapped for the fuzz only; no module patches bytes
                                // inside its 44 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossShroom_Inject();
    BossDLordFx_Inject();
    BossDLord_Inject();
    Boss55_Inject();
    Boss52_Inject();
    BossSg_Inject();            // round 11 group BSG (fights 29, 31..33, kinds 34..38 and 40, the effect task F6,
                                // 0x43CDE0..0x43E535): its clones' calls re-aimed at the boss harness's recorders, its
                                // kinds' and F6's tables swapped for the fuzz only; no module patches bytes inside its
                                // 53 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossWeretigrFx_Inject();
    BossWeretigr_Inject();
    BossMikba_Inject();
    BossGisshan_Inject();
    BossGarr2_Inject();
    BossDolphin_Inject();
    BossDZombie_Inject();
    Boss32_Inject();
    Boss31_Inject();
    if (bof3::WantsShadow("boss_sf")) boss_sf::SelfTest();            // round 11 group BSF (fights 27, 28, kinds 33 and 62, fight 26's count task and the
                                // Gazer's effect task, 0x43C480..0x4406D7): its clones' calls re-aimed at the boss
                                // harness's recorders, the kinds' and tasks' tables swapped for the fuzz only; no
                                // module patches bytes inside its 54 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossMyria_Inject();
    BossGazerFx_Inject();
    BossGazer_Inject();
    Boss28_Inject();
    Boss25_Inject();
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
    BattleMenuWinFromBattleE7_Inject();
    MenuListFromBattleE7_Inject();
    if (bof3::WantsShadow("battle_e1")) battle_e1::SelfTest();          // round 12 group BE1 (BATE's tally and equipment screens, the command menu's held
                                // steps and auto battle, action kind 3, the ability notice, the result's counts and
                                // level gain, the loss screen, 0x42D7A0..0x432B6A): its clones' calls re-aimed at the
                                // boss harness's recorders, its dispatchers' tables swapped for the fuzz only; no module
                                // patches bytes inside its 41 (DIVERGENCE.md, cheats.cpp; DIV-0020 reads 0x42E09D,
                                // past BattleExtra_EquipLeave's jmp): order does not matter
    BattleBattleResult_Inject();
    BattleLoss_Inject();
    BattleE5_Inject();          // round 12 group BE5 (the enemy AI helpers, Effect_Handlers slots 22 / 23 / 38,
                                // BattleForm_ApplyStats, the Dragon command's run, 0x44B240..0x451471): its clones'
                                // calls re-aimed at the boss harness's recorders, the Dragon run's six step tables
                                // swapped for the fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    DragonCmdFromBattleE5_Inject();
    if (bof3::WantsShadow("battle_e6")) battle_e6::SelfTest();          // round 12 group BE6 (the transformation, the gene cost, BattleFx_Dispatch slots 15,
                                // 16, 18, the stat rebuild, the member roll, the AP pop-up, the Field_Slots debts,
                                // BMAGIC's four map cells; 0x451480..0x4552F5, 0x4CEB40..0x4CF4A3): its clones' calls
                                // re-aimed at the boss harness's recorders, the tasks' tables swapped for the fuzz
                                // only; no module patches bytes inside its 39 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    if (bof3::WantsShadow("battle_e4")) battle_e4::SelfTest();          // round 12 group BE4 (the battle engine's 0x444660..0x44AAC9: damage, the pace and
                                // target helpers, the item command's states 5..9, the escape): its clones' calls
                                // re-aimed at the boss harness's recorders, its four dispatchers' tables swapped for
                                // the fuzz only; after BossHarnessEh_Inject, whose self-test copies 0x4457F0; no
                                // module patches bytes inside its 56 (DIVERGENCE.md, cheats.cpp)
    BattleBanner_Inject();
    if (bof3::WantsShadow("battle_e2")) battle_e2::SelfTest();          // round 12 group BE2 (0x433650..0x437030: the effect tasks of BattleFx_Dispatch slots
                                // 9, 10, 12, 13, 14, 17 and the watch's state 3, the enemy's action pick and targets,
                                // EnemyOp_Steps 4 / 5, EnterSubs 1, ActSubs 3 / 5): its clones' calls re-aimed at the boss
                                // harness's recorders, the EnemyOp tables swapped for the fuzz only; no module patches
                                // bytes inside its 48 (DIVERGENCE.md, cheats.cpp): order does not matter
    if (bof3::WantsShadow("battle_e3")) battle_e3::SelfTest();          // round 12 group BE3 (the enemy ops EnemyOp_Steps 7..9, the party objects' states 6,
                                // 10, 11, 26, two party helpers, 0x437030..0x442F97): its clones' calls re-aimed at the
                                // boss harness's recorders, its sub-tables swapped for the fuzz only; after
                                // BossHarnessEh_Inject, whose self-test copies 0x441A10 / 0x441A30; no module patches
                                // bytes inside its 49 (DIVERGENCE.md, cheats.cpp)
    BossMyriaFx_Inject();
    BattleObj_Inject();
    if (bof3::WantsShadow("field_c2")) field_c2::SelfTest();           // round 12 group FC2 (the field core's 0x46BBF0..0x46D5ED: effect kinds 0x30, 0x34,
                                // 0x3A and 0x41): its clones' calls re-aimed at the scenario harness's recorders, the
                                // kinds' eight state tables swapped for the fuzz only; after ScenarioHarnessFh_Inject
                                // (none of its thirteen is FC2's); no module patches bytes inside its 44
                                // (DIVERGENCE.md, cheats.cpp): order does not matter otherwise
    EffectKind34FromFieldC2_Inject();
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
    AreaMap_Inject();
    if (bof3::WantsShadow("field_c3")) field_c3::SelfTest();           // round 12 group FC3 (object steering, op E7, the fade cases, mode 8's and 11's
                                // frames, the field core's state-2 sub-states 0, 1, 3..8 and their steps;
                                // 0x5172C0..0x5195F9, 0x525390..0x526DAF): its clones' calls re-aimed at the scenario
                                // harness's recorders, its ten tables swapped for the fuzz only; after
                                // ScenarioHarnessFh_Inject, whose self-test copies 0x525CC0 (FieldCore_Up), and after
                                // ObjectKinds_Inject, whose copy of Field_ObjectFadeOut holds the four fade cases; no
                                // module patches bytes inside its 63 (DIVERGENCE.md, cheats.cpp)
    Field3_Inject();
    if (bof3::WantsShadow("field_e1")) field_e1::SelfTest();           // round 12 group FE1 (the field engine 0x52D080..0x533BA0: the panel draws, the
                                // leader states 6, 8, 11, 12 and the jump, a field object's content, the zenny found,
                                // the cells around a sprite, the gateway exit, the party's placements, the pending
                                // jump's members): its clones' calls re-aimed at the scenario harness's recorders,
                                // its five tables swapped for the fuzz only; after ScenarioHarnessFh_Inject, whose
                                // self-test copies 0x52F980 and 0x52D880; no module patches bytes inside its 45
                                // (DIVERGENCE.md, cheats.cpp)
    Field4_Inject();
    Field6_Inject();
    FieldO_Inject();            // round 12 group FO (the field engine's 0x5738A0..0x57CD89: eight menu panels, the
                                // movement ops 87 / 88 / DB / E9 / F9 and their states, the placement ops 3x 4x 6x 7x
                                // Ax, thirteen event conditions, EventScript_SkipIf, ObjTrio_ClearBit40): its clones'
                                // calls re-aimed at the scenario harness's recorders; after ScenarioHarnessFh_Inject,
                                // whose self-test copies 0x57C1A0, 0x57C230, 0x57C8E0 (FO's); after YesNoLayout_Inject,
                                // whose DIV-0029 byte at 0x576A48 Menu_DrawSaveSlot reads back
    FieldMoveScript_Inject();
    Menu_Inject();
    MoveCmd_Inject();
    FieldEventScript_Inject();
    if (bof3::WantsShadow("field_c1")) field_c1::SelfTest();           // round 12 group FC1 (the field core's first half, 0x461800 and 0x469D10..0x46BBF0:
                                // the Config row label, effect kinds 4, 0x14, 0x17, 0x19, 0x1B, 0x30..0x32, 0x37,
                                // 0x3C and two of kind 6's ticks): its clones' calls re-aimed at the scenario
                                // harness's recorders, its five state tables swapped for the fuzz only; after
                                // ScenarioHarnessFh_Inject (none of its thirteen is FC1's). config_text.cpp patches
                                // operands inside 0x461800 (DIV-0015 / DIV-0017), which ours reads in place at each
                                // call: order does not matter
    EffectKind34FromFieldC1_Inject();
    EffectKind17FromFieldC1_Inject();
    EffectKind06FromFieldC1_Inject();
    TitleFromFieldC1_Inject();
    EffectGte_Inject();         // round 13 stage-A group EGT (0x494060..0x494270: the map camera into the GTE, a
                                // world point projected, a matrix's diagonal of ones, a size at a point's depth):
                                // its clones' calls re-aimed at the scenario harness's recorders; every caller
                                // that is ours calls it by the address it had, so order does not matter; no
                                // module patches bytes inside its four (DIVERGENCE.md, cheats.cpp)
    EffectEffectGteFromEffectGte_Inject();
    ScenarioHarnessEkh_Inject(); // round 13 group EKH: the scenario harness's effect mode proved on Capcom's code, eight
                                // rows of round thirteen's cut on both sides - injects nothing, takes nothing; after
                                // every scenario group, so their random streams are the ones they had; every effect
                                // group's inject goes after it, whose self-test copies 0x462BC0 (E1A), 0x46F2B0,
                                // 0x46F7D0 (E1D), 0x4FD470 (E5A), 0x500D20 (E5B), 0x479970 (E2E), 0x4857C0 (E3C),
                                // 0x472770 (E2A) from the image - all ours now, named by their bof3::addr constants
                                // (the originals' addresses), and run "in place" as Capcom's only because those
                                // groups inject below (docs/scenario_harness.md sections 8.8 and 8.10)
    if (bof3::WantsShadow("effect_1f")) effect_1f::SelfTest();          // round 13 group E1F (0x52A6C0..0x52D07C: the extra-slot menu on effect record 6,
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
    EffectKind17FromEffect1b_Inject();
    EffectKind11_Inject();
    EffectKind0fFromEffect1b_Inject();
    EffectKind06FromEffect1b_Inject();
    Effect1G_Inject();          // round 13 group E1G (0x594060..0x594D8A: the item-trade screen's rest - its fourth
                                // run step, its leave state and steps, the backdrop, list, ingredient and count
                                // windows, the ingredient test, the row count - and Item_DrawIcon): its clones' calls
                                // re-aimed at the scenario harness's recorders; after ScenarioHarnessEkh_Inject (none
                                // of its eight is E1G's) and FieldE2_Inject (whose trade states call these by the
                                // addresses they had); no module patches bytes inside the fourteen
    if (bof3::WantsShadow("effect_1d")) effect_1d::SelfTest();          // round 13 group E1D (0x46F2B0..0x4702F6: effect kinds 0x21..0x27, their dispatchers,
                                // states and two draw helpers): its clones' calls re-aimed at the scenario harness's
                                // recorders, its seven state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, whose self-test copies 0x46F2B0 and 0x46F7D0; no module
                                // patches bytes inside its 30 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind27_Inject();
    EffectKind21FromEffect1d_Inject();
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
    EffectWorldMapFromEffect1a_Inject();
    EffectKind0fFromEffect1a_Inject();
    EffectKind09FromEffect1a_Inject();
    if (bof3::WantsShadow("effect_1c")) effect_1c::SelfTest();          // round 13 group E1C (0x46A850..0x46F2A6: effect kinds 0x36, 0x3C, 0x70, 0x1C..0x20):
                                // its clones' calls re-aimed at the scenario harness's recorders, its seven state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight is E1C's); no module patches bytes inside its 53 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    EffectShardsFromEffect1c_Inject();
    EffectKind34FromEffect1c_Inject();
    EffectKind20_Inject();
    EffectKind1d_Inject();
    EffectKind1c_Inject();
    EffectKind17FromEffect1c_Inject();
    Effect2G_Inject();          // round 13 group E2G (0x47DBE0..0x47FD92: effect kinds 0x15, 0x54, 0x55, 0x57, 0x5A,
                                // 0x5B, 0x66 and kind 0x18's sub-kind 0x20 - dispatchers, states, draws - and the
                                // dispatchers of kinds 0x5D..0x5F): its clones' calls re-aimed at the scenario harness's
                                // recorders, its eleven state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E2G's); no module patches bytes
                                // inside its 58 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind66FromEffect2g_Inject();
    EffectKind5fFromEffect2g_Inject();
    EffectKind5dFromEffect2g_Inject();
    EffectKind54_Inject();
    if (bof3::WantsShadow("effect_2b")) effect_2b::SelfTest();          // round 13 group E2B (0x4731A0..0x474F34: effect kinds 0x2F, 0x33, 0x35, 0x38, 0x39,
                                // 0x3B, 0x3D): its clones' calls re-aimed at the scenario harness's recorders, its eight
                                // state tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight is E2B's) and Effect1C_Inject (EffectKind1E_ShardInit, called by name); no
                                // module patches bytes inside its 52 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind3dFromEffect2b_Inject();
    EffectKind3b_Inject();
    EffectKind38_Inject();
    EffectKind35_Inject();
    EffectKind33_Inject();
    EffectKind2f_Inject();
    Effect2D_Inject();          // round 13 group E2D (0x4771B0..0x4789C1: effect kinds 0x45..0x49, their dispatchers,
                                // states and draw helpers): its clones' calls re-aimed at the scenario harness's
                                // recorders, its ten state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E2D's); no module patches
                                // bytes inside its 54 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind48FromEffect2d_Inject();
    EffectKind46_Inject();
    EffectKind45_Inject();
    Effect2C_Inject();          // round 13 group E2C (0x474F40..0x4771A5: effect kinds 0x3E, 0x3F, 0x40, 0x42, 0x43,
                                // 0x44 and 0x6B, their dispatchers, states and draw helpers): its clones' calls
                                // re-aimed at the scenario harness's recorders, its nine tables swapped for the fuzz
                                // only; after ScenarioHarnessEkh_Inject (none of its eight is E2C's); no module
                                // patches bytes inside its 53 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind44_Inject();
    EffectKind43_Inject();
    EffectKind3dFromEffect2c_Inject();
    if (bof3::WantsShadow("effect_2f")) effect_2f::SelfTest();          // round 13 group E2F (0x47B7D0..0x47DBD1: effect kinds 0x4F, 0x50, 0x51, 0x52, 0x53,
                                // 0x56 and kind 0x4E's disc): its clones' calls re-aimed at the scenario harness's
                                // recorders, its eight state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight is E2F's); no module patches bytes
                                // inside its 62 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind66FromEffect2f_Inject();
    EffectKind51_Inject();
    EffectKind4eFromEffect2f_Inject();
    if (bof3::WantsShadow("effect_2e")) effect_2e::SelfTest();          // round 13 group E2E (0x4789D0..0x47B7CF: kind 0x48's states 7..12, effect kinds
                                // 0x4A..0x4E, the sphere, the angle mean): its clones' calls re-aimed at the scenario
                                // harness's recorders, its ten state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, which copies E2E's 0x479970; no module patches bytes
                                // inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind48FromEffect2e_Inject();
    EffectKind4eFromEffect2e_Inject();
    EffectKind4d_Inject();
    if (bof3::WantsShadow("effect_2a")) effect_2a::SelfTest();          // round 13 group E2A (0x470300..0x473192: effect kinds 0x28..0x2E, their dispatchers,
                                // states and draws, and the pool's specks, sparks and drops): its clones' calls
                                // re-aimed at the scenario harness's recorders, its seven state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject, whose self-test copies 0x472770,
                                // and Effect1C_Inject, whose kinds 0x1C / 0x1D call 0x471D10 and 0x471E20; no module
                                // patches bytes inside its 65 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectSpecks_Inject();
    EffectKind2e_Inject();
    EffectKind2d_Inject();
    EffectKind2b_Inject();
    EffectKind2a_Inject();
    EffectKind29_Inject();
    EffectKind28_Inject();
    if (bof3::WantsShadow("effect_3b")) effect_3b::SelfTest();          // round 13 group E3B (0x4823D0..0x48404A: effect kinds 0x63, 0x65, 0x67, 0x69, 0x6C -
                                // dispatchers, states, draws): its clones' calls re-aimed at the scenario harness's
                                // recorders, its nine state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E3B's); no module patches bytes
                                // inside its 49 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind6eFromEffect3b_Inject();
    EffectKind69FromEffect3b_Inject();
    EffectKind65_Inject();
    EffectKind63_Inject();
    if (bof3::WantsShadow("effect_3d")) effect_3d::SelfTest();          // round 13 group E3D (0x485CB0..0x48801A: effect kinds 0x76..0x7D and 0x7F..0x82,
                                // their dispatchers, states and draws, area 170's dial map): its clones' calls
                                // re-aimed at the scenario harness's recorders, its eleven state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E3D's)
                                // and the wave-two groups it calls (E2E's angle mean, E2F's trail cap); DrawPool
                                // re-aims the immediate at 0x486EBD inside EffectKind7D_SetMap (DIV-0062), after
                                // every self-test; no other module patches bytes inside its 68
    EffectKind82FromEffect3d_Inject();
    EffectKind81_Inject();
    EffectKind80_Inject();
    EffectKind7d_Inject();
    EffectKind78_Inject();
    EffectKind77_Inject();
    if (bof3::WantsShadow("effect_4d")) effect_4d::SelfTest();          // round 13 group E4D (0x48C990..0x48DF87: effect kinds 0x91, 0x94..0x98, 0x9A and the
                                // screen tint 0x48CA90 - dispatchers, states, draws): its clones' calls re-aimed at
                                // the scenario harness's recorders, its seven state tables swapped for the fuzz only;
                                // after ScenarioHarnessEkh_Inject (none of its eight rows is E4D's) and before
                                // Widescreen_ArmFills (its three fills compare the original's 320 x 240); no
                                // module patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind9aFromEffect4d_Inject();
    EffectKind97_Inject();
    EffectKind95_Inject();
    EffectKind94_Inject();
    EffectKind91_Inject();
    if (bof3::WantsShadow("effect_3c")) effect_3c::SelfTest();          // round 13 group E3C (0x484050..0x485CA0: effect kinds 0x6D, 0x6E, 0x6F, 0x72..0x75,
                                // their dispatchers, states and draws, and the debris draw and set-up kinds 0x1E,
                                // 0x1F and 0x4B share): its clones' calls re-aimed at the scenario harness's
                                // recorders, its nine state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, whose self-test copies 0x4857C0, and Effect1C_Inject /
                                // Effect2E_Inject, which call 0x485030 and 0x4851E0 by address; no module patches
                                // bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind73_Inject();
    EffectKind6f_Inject();
    EffectKind6eFromEffect3c_Inject();
    EffectDebris_Inject();
    Effect3A_Inject();          // round 13 group E3A (0x4801F0..0x4823C2: effect kinds 0x60, 0x61, 0x62, 0x64 and
                                // 0x68 - dispatchers, states and draws): its clones' calls re-aimed at the scenario
                                // harness's recorders, its five state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E3A's); no module patches bytes
                                // inside its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind64_Inject();
    EffectKind5fFromEffect3a_Inject();
    if (bof3::WantsShadow("effect_4c")) effect_4c::SelfTest();          // round 13 group E4C (0x48B200..0x48C985: effect kinds 0x8D, 0x8E, 0x8F, 0x90, 0x93
                                // and 0x99 - dispatchers, states, pools and draws): its clones' calls re-aimed at the
                                // scenario harness's recorders, its six state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E4C's) and Effect3C_Inject,
                                // whose EffectKind6E_FindShard it calls by name; no module patches bytes inside its
                                // 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind8f_Inject();
    EffectKind8e_Inject();
    EffectKind8d_Inject();
    if (bof3::WantsShadow("effect_4f")) effect_4f::SelfTest();          // round 13 group E4F (0x491D70..0x494026: effect kinds 0xAA..0xB1, 0xB9 and 0xBA -
                                // dispatchers, states and draws): its clones' calls re-aimed at the scenario harness's
                                // recorders, its ten state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E4F's) and Effect3A_Inject /
                                // Effect1C_Inject, which call 0x493B50, 0x493C60 and 0x493090 by address; no module
                                // patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp, widescreen.cpp); before
                                // Widescreen_ArmFills (EffectKindAF_DrawScreen's fill, DIV-0041's 0x493308, compares
                                // the original's 320 x 240)
    EffectKindBaFromEffect4f_Inject();
    EffectKindB9_Inject();
    EffectKindB1_Inject();
    EffectKindAeFromEffect4f_Inject();
    EffectKindAcFromEffect4f_Inject();
    EffectKindAbFromEffect4f_Inject();
    EffectKindA8FromEffect4f_Inject();
    if (bof3::WantsShadow("effect_4e")) effect_4e::SelfTest();          // round 13 group E4E (0x48DF90..0x491C96: effect kinds 0x9B, 0x9C and 0xA0 whole,
                                // kind 0x9E's dispatcher and draws, the dispatchers of 0xA1..0xA3 and 0xA7..0xA9):
                                // its clones' calls re-aimed at the scenario harness's recorders, its ten state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight rows is E4E's) and Effect3A_Inject (EffectKind64_DrawSpark called by name);
                                // no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKindA8FromEffect4e_Inject();
    EffectKindA0FromEffect4e_Inject();
    EffectKind9eFromEffect4e_Inject();
    EffectKind9cFromEffect4e_Inject();
    EffectKind9aFromEffect4e_Inject();
    Effect4B_Inject();          // round 13 group E4B (0x489030..0x48B1FC: effect kinds 0x88..0x8C, 0x9D, 0x9F, 0xA4 -
                                // dispatchers, states, draws - and kind 0x87's helpers): its clones' calls re-aimed at
                                // the scenario harness's recorders, its eight state tables swapped for the fuzz only;
                                // after ScenarioHarnessEkh_Inject (none of its eight rows is E4B's) and before
                                // Widescreen_ArmFills (EffectKind89_DrawTint's fill, DIV-0041's 0x489D47, compares the
                                // original's 320 x 240); no module patches bytes inside its 66 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    EffectKind9d_Inject();
    EffectKind8c_Inject();
    EffectKind8a_Inject();
    EffectKind88_Inject();
    EffectKind87FromEffect4b_Inject();
    if (bof3::WantsShadow("effect_4a")) effect_4a::SelfTest();          // round 13 group E4A (0x488020..0x48902E and 0x433640: effect kinds 0x82 (states
                                // 11..23), 0x83..0x87 - dispatchers and states): its clones' calls re-aimed at the
                                // scenario harness's recorders, its five state tables swapped for the fuzz only;
                                // after ScenarioHarnessEkh_Inject (none of its eight rows is E4A's) and Effect3D_Inject
                                // (kind 0x82's first states, whose table holds eight of these); no module patches
                                // bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind87FromEffect4a_Inject();
    EffectKind85_Inject();
    EffectKind84_Inject();
    EffectKind83_Inject();
    EffectKind82FromEffect4a_Inject();
    if (bof3::WantsShadow("effect_5g")) effect_5g::SelfTest();          // round 13 group E5G (0x50AF90..0x50C0CA: kind 0x18's sub-kinds 0x2B, 0x2C, 0x36,
                                // 0x3A, 0x4A - sub-state dispatchers, states, panel draws): its clones' calls re-aimed
                                // at the scenario harness's recorders, its four sub-state tables swapped for the fuzz
                                // only; after ScenarioHarnessEkh_Inject (none of its eight rows is E5G's) and before
                                // Widescreen_ArmFills (sub-kind 0x36's fill compares the original's 320 x 240); no
                                // module patches bytes inside its 24 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub2b_Inject();
    if (bof3::WantsShadow("effect_5a")) effect_5a::SelfTest();          // round 13 group E5A (0x4FD2E0..0x4FF318: kind 0x18's sub-kinds 1, 4..0xA, 0x1A and
                                // 0x1F - sub-state dispatchers, states and draws - and Gfx_ClutStripCopy16): its
                                // clones' calls re-aimed at the scenario harness's recorders, its eight sub-state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject, whose self-test
                                // copies 0x4FD470 (EffectKind18_04_Run); no module patches bytes inside its 55
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18_Inject();
    if (bof3::WantsShadow("effect_5b")) effect_5b::SelfTest();          // round 13 group E5B (0x4FF320..0x5014F2: kind 0x18's sub-kinds 0x0B..0x0F, 0x13 and
                                // 0x4F - dispatchers, states, draws): its clones' calls re-aimed at the scenario
                                // harness's recorders, its seven sub-state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, whose self-test copies 0x500D20 by address; no module
                                // patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub0e_Inject();
    EffectKind18Sub0d_Inject();
    EffectKind18Sub0c_Inject();
    if (bof3::WantsShadow("effect_5f")) effect_5f::SelfTest();          // round 13 group E5F (0x508CC0..0x50AF8D: kind 0x18's sub-kinds 0x27, 0x28, 0x29, 0x2A,
                                // 0x3C, 0x42, 0x48, 0x49 and 0x4B / 0x4C - dispatchers, states, draws): its clones'
                                // calls re-aimed at the scenario harness's recorders, its eight state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E5F's) and
                                // before DrawPool_Grow (EffectKind18Sub42_Mark is one of DIV-0062's owned users); no
                                // module patches bytes inside its 49 but DrawPool_Grow's five sites in 0x509850
    EffectKind18Sub42_Inject();
    EffectKind18Sub2a_Inject();
    EffectKind18Sub29_Inject();
    EffectKind18Sub27_Inject();
    if (bof3::WantsShadow("effect_5e")) effect_5e::SelfTest();          // round 13 group E5E (0x506A10..0x508CBB: effect kind 0x18's sub-kinds 0x23..0x26,
                                // 0x39 and 0x3F - dispatchers by +2, sub-states, draws): its clones' calls re-aimed
                                // at the scenario harness's recorders, its six sub-state tables swapped for the fuzz
                                // only; after ScenarioHarnessEkh_Inject (none of its eight rows is E5E's) and before
                                // Widescreen_ArmFills (its two full-frame fills, DIV-0041's 0x507BDC and 0x507CE3,
                                // compare the original's 320 x 240); no module patches bytes inside its 54
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub3f_Inject();
    EffectKind18Sub26_Inject();
    EffectKind18Sub23_Inject();
    if (bof3::WantsShadow("effect_5d")) effect_5d::SelfTest();          // round 13 group E5D (0x503DE0..0x506A00: effect kind 0x18's sub-kinds 0x14, 0x18,
                                // 0x19, 0x1B..0x1E, 0x21, 0x22, 0x43, 0x52, 0x68 and four of sub-kind 0x17's helpers):
                                // its clones' calls re-aimed at the scenario harness's recorders, its six state tables
                                // swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is
                                // E5D's; kEffectOverrides' 0x503FA0 row stands in for E5C's raw calls); no module
                                // patches bytes inside its 52 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub22_Inject();
    EffectKind18Sub21_Inject();
    EffectKind18Sub1c_Inject();
    if (bof3::WantsShadow("effect_5c")) effect_5c::SelfTest();          // round 13 group E5C (0x501500..0x503DDD: effect kind 0x18's sub-kinds 0x10, 0x11,
                                // 0x12, 0x15, 0x16, 0x17, 0x50, 0x56..0x58 - dispatchers by +2, sub-states, draws):
                                // its clones' calls re-aimed at the scenario harness's recorders, its nine state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight rows is E5C's); no module patches bytes inside its 62 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    EffectKind18Sub17_Inject();
    EffectKind18Sub12_Inject();
    EffectKind18Sub11_Inject();
    EffectKind18Sub10_Inject();
    EffectKind18Sub0f_Inject();
    if (bof3::WantsShadow("effect_6d")) effect_6d::SelfTest();          // round 13 group E6D (0x514270..0x516B2E: kind 0x18's sub-kinds 0x5C, 0x5D, 0x5E,
                                // 0x61..0x65 - dispatchers by +2 and Cond_ByteFE, states, draws): its clones' calls
                                // re-aimed at the scenario harness's recorders, its six state tables swapped for the
                                // fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E6D's); no
                                // module patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub65_Inject();
    EffectKind18Sub64_Inject();
    EffectKind18Sub61_Inject();
    EffectKind18Sub5d_Inject();
    if (bof3::WantsShadow("effect_6a")) effect_6a::SelfTest();          // round 13 group E6A (0x50C0D0..0x50E3FA: effect kind 0x18's sub-kinds 0x2D, 0x2E..0x32,
                                // 0x3E and 0x4A's state 1 - dispatchers by +2, sub-states, draws): its clones' calls
                                // re-aimed at the scenario harness's recorders, its seven sub-state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E6A's) and
                                // Effect5G_Inject (0x50C0D0 tail-jumps to its EffectKind18Sub2C_Draw); no module
                                // patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub32_Inject();
    EffectKind18Sub31_Inject();
    EffectKind18Sub30_Inject();
    EffectKind18Sub2f_Inject();
    EffectKind18Sub2e_Inject();
    EffectKind18Sub2d_Inject();
    EffectKind18Sub2c_Inject();
    if (bof3::WantsShadow("effect_6c")) effect_6c::SelfTest();          // round 13 group E6C (0x510C90..0x51426B: effect kind 0x18's sub-kinds 0x44 (frame
                                // and sky), 0x45, 0x51, 0x53, 0x55, 0x59, 0x5A, 0x5B, 0x66, and AreaMap_CornerHeight):
                                // its clones' calls re-aimed at the scenario harness's recorders, its six state tables
                                // swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is
                                // E6C's) and before Widescreen_ArmFills (sub-kind 0x44's red fill is DIV-0041's) and
                                // DrawPool_Grow (five DIV-0062 sites); no module patches bytes inside its 50 but
                                // draw_pool.cpp's six DrawItems immediates
    EffectKind18Sub5b_Inject();
    EffectKind18Sub59_Inject();
    EffectKind18Sub55_Inject();
    EffectKind18Sub51_Inject();
    EffectKind18Sub45_Inject();
    if (bof3::WantsShadow("effect_6b")) effect_6b::SelfTest();          // round 13 group E6B (0x50E400..0x510C8A: effect kind 0x18's sub-kinds 0x33..0x35,
                                // 0x37, 0x38, 0x3B, 0x3D, 0x40, 0x41, 0x44, 0x54 - dispatchers by +2, sub-states,
                                // draws): its clones' calls re-aimed at the scenario harness's recorders, its nine
                                // sub-state tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of
                                // its eight rows is E6B's) and before Widescreen_ArmFills (sub-kind 0x54's fill,
                                // DIV-0041's 0x50F7B5, compares the original's 320 x 240); no module patches bytes
                                // inside its 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    EffectKind18Sub40_Inject();
    EffectKind18Sub3b_Inject();
    EffectKind18Sub35_Inject();
    EffectKind18Sub33_Inject();
    if (bof3::WantsShadow("rest_0a")) rest_0a::SelfTest();            // round 14 stage-A group R0A (0x51C390..0x524E4E: the party sets' field actions' seven
                                // shared helpers - the probes two steps ahead, the kind-0x30 effect object ahead and
                                // the members in reach of it, an effect object of kind 0x34 on a cell, the side
                                // probes): its clones' calls re-aimed at the scenario harness's recorders; after
                                // every harness's inject; every caller that is ours (PartyAction5_Form0Begin) calls
                                // it by the address it had, so order does not matter; no module patches bytes inside
                                // its seven (DIVERGENCE.md, cheats.cpp)
    if (bof3::WantsShadow("rest_1b")) rest_1b::SelfTest();            // round 14 wave-one group R1B (0x51D710..0x51F202: party sets 2..6's field actions -
                                // the form, state and step dispatchers, the states of sets 3..5, four cell
                                // handlers): its clones' calls re-aimed at the scenario harness's recorders, its 29
                                // tables swapped for the fuzz only; after every harness's inject and R0A's (it calls
                                // R0A's helpers by name); no module patches bytes inside its 47 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    if (bof3::WantsShadow("rest_1c")) rest_1c::SelfTest();            // round 14 wave-one group R1C (0x51F210..0x520E08: party sets 6..9's field actions -
                                // the dispatchers by form, state and step, their probe / resolve states, three cell
                                // pickups, two cell hits, the shared turn, tick and wait states): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 28 dispatch tables swapped for the
                                // fuzz only; after Rest0A_Inject (it calls R0A's helpers by name) and every harness's
                                // inject; no module patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    if (bof3::WantsShadow("rest_1g")) rest_1g::SelfTest();            // round 14 wave-one group R1G (0x5289A0..0x52CD46: the fishing spot - the leader's
                                // state 9 stages 1, 10, 11 and their steps, the menu's data and rule pages, the
                                // stages' helpers; game mode 8's fish): its clones' calls re-aimed at the scenario
                                // harness's recorders, its six tables swapped for the fuzz only; after every
                                // harness's inject; ours that call it (E1E, E1F) call it by the address it had;
                                // before FishingText_Arm, which patches no byte inside its 45 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    if (bof3::WantsShadow("rest_1f")) rest_1f::SelfTest();            // round 14 group R1F (0x523ED0..0x52899B: party sets 16..18's field actions, a
                                // raised sprite's cell ahead, the leader's state 9's stage 0): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 24 tables swapped for the fuzz
                                // only; after every harness's inject and Rest0A_Inject (its callers reach R0A by the
                                // address); no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    LeaderPanel_Inject();
    FieldCore_Inject();
    if (bof3::WantsShadow("rest_1d")) rest_1d::SelfTest();            // round 14 wave-one group R1D (0x520E10..0x5226C1: party sets 9..12's field actions -
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
    Menu2_Inject();
    Field7_Inject();
    EventCond_Inject();
    if (bof3::WantsShadow("rest_1e")) rest_1e::SelfTest();            // round 14 group R1E (0x5226D0..0x523EC2: the field actions of party sets 13, 14,
                                // 15 and set 16's forms 0 and 1 - the dispatchers by the form word and the state
                                // bytes, the turn-and-probe, resolve, cell-pickup and strike states): its clones'
                                // calls re-aimed at the scenario harness's recorders, its 28 state tables swapped for
                                // the fuzz only; after Rest0A_Inject (it calls R0A's helpers by name) and every
                                // harness's inject; no module patches bytes inside its 47 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    if (bof3::WantsShadow("rest_1a")) rest_1a::SelfTest();            // round 14 wave-one group R1A (0x51BA80..0x51D70C: party-member states 4, 6, 8 and
                                // party sets 0..2's field actions - their dispatchers by form, state and step, the
                                // form actions' turns, the cell pickups and set 2's cell strike): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 26 tables swapped for the fuzz
                                // only; after Rest0A_Inject (it calls R0A's helpers by name) and every harness's
                                // inject; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    PartyAction_Inject();
    Member_Inject();
    if (bof3::WantsShadow("rest_2a")) rest_2a::SelfTest();            // round 14 wave-two group R2A (0x5372E0..0x537F1B: the field frame's listed-bank
                                // screen pass, Char_GainHp / Char_LoseAp, Area_ObjectHandler and the four object
                                // handlers of its fallback table Area_ObjectFallbacks with their states and spawns):
                                // its clones' calls and stack-table immediates re-aimed at the scenario harness's
                                // recorders, Area_ObjectFallbacks and Area74_Handlers swapped for the fuzz only; after
                                // every harness's inject and wave one's; no module patches bytes inside its 22
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp)
    LinkedObject_Inject();
    Mode11_Inject();
    if (bof3::WantsShadow("rest_2g")) rest_2g::SelfTest();            // round 14 wave-two group R2G (0x597FA0..0x59AA77: record handler 4's kind 0 and the
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
    MasterQuit_Inject();
    FieldMenuStatus_Inject();
    FieldMenu_Inject();
    FieldAbility_Inject();
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
    ShopWin2FromRest2h_Inject();
    ShopWin2_Inject();
    DInput_Inject();
    MenuList_Inject();
    BattleMenuWin_Inject();
    Rest2F_Inject();            // round 14 wave-two group R2F (0x58ED40..0x596F98: the field menu's Tactics and
                                // Config steps, the Ability screen's helpers, window-record handlers 1 and 2 and
                                // window kind 1): its clones' calls re-aimed at the scenario harness's recorders, its
                                // 15 state tables swapped for the fuzz only; after Widescreen_Inject (DIV-0041's
                                // bound inside MenuSlide_LeftOff170, read back) and BattleDraw_Inject (DIV-0059's
                                // title call inside Win2_DrawItemList, read back) and every harness's inject
    Window_Inject();
    ConfigMenu_Inject();
    FieldAbility2_Inject();
    Rest2C_Inject();            // round 14 wave-two group R2C (0x57F340..0x58699F: the inn's, save point's and rest's
                                // last states, the save block's builder and Save_QuickWrite, the shop's browse and
                                // sell modes and four ShopMode dispatchers, the master's talk and its panels, the
                                // figure record's moves): its clones' calls re-aimed at the scenario harness's
                                // recorders, its 14 state tables swapped for the fuzz only; after every harness's
                                // inject; ours that call it (Game_WndProc, FieldTail_LoadBank, effect_2g, effect_3d)
                                // call it by the address it had; before FishingText_Arm; no module patches bytes
                                // inside its 61 (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp,
                                // yes_no_layout.cpp)
    FieldSaveFromRest2c_Inject();
    ShopTrade_Inject();
    ShopResist_Inject();
    ShopBrowse_Inject();
    SharedList_Inject();
    FieldSave_Inject();
    Shisu_Inject();
    MasterTalk2_Inject();
    MasterTalk_Inject();
    PartyForm_Inject();
    if (bof3::WantsShadow("rest_3a")) rest_3a::SelfTest();            // round 14 wave-three group R3A (0x404180..0x4378AA: area 33's world-map frame
                                // states, BATE's root and three equipment helpers, BattleEnd_Steps[2] / [3] and
                                // ExitSteps[3], kind-0 battle-task slots 2, 4, 5, 11, 12 and the actor watch's
                                // states 1 / 4, BattleBossFx_Dispatch, the enemy animation helpers, New Game's
                                // records): its clones' calls re-aimed at the boss harness's recorders, its five
                                // step tables swapped for the fuzz only; after every harness's inject and every
                                // module whose functions it calls; DIV-0020's check of 0x437834 / 0x437891 reads
                                // bytes the inject's jmp at 0x437820 leaves in place; no module patches bytes
                                // inside its 45 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    BattleExtra_Inject();
    WorldMap33_Inject();
    EnemyOp_Inject();
    BattleTask_Inject();
    BattleFx_Inject();
    BattleAction_Inject();
    Battle_Inject();
    if (bof3::WantsShadow("rest_3c")) rest_3c::SelfTest();            // round 14 wave-three group R3C (0x44D000..0x44E4AA: Effect_Handlers slots 50..85 and
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
    EffectKind48FromRest3e_Inject();
    EffectKind5fFromRest3e_Inject();
    EffectKind5dFromRest3e_Inject();
    EffectShardsFromRest3e_Inject();
    EffectKind34FromRest3e_Inject();
    EffectKind21FromRest3e_Inject();
    EffectKind17FromRest3e_Inject();
    EffectKind06FromRest3e_Inject();
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
    Quake_Inject();
    EffectKindBaFromRest3g_Inject();
    EffectKindAeFromRest3g_Inject();
    EffectKindAcFromRest3g_Inject();
    EffectEffectGteFromRest3g_Inject();
    TransitionFromRest3g_Inject();
    Battle5FromRest3g_Inject();
    Battle4FromRest3g_Inject();
    EffectKind18Sub44_Inject();
    EffectKind0f2_Inject();
    Area109_Inject();
    if (bof3::WantsShadow("rest_3d")) rest_3d::SelfTest();            // round 14 wave-three group R3D (0x44E4B0..0x44FF0E: Effect_Handlers slots 112..129,
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
    EffectKindAbFromRest3f_Inject();
    EffectKindA8FromRest3f_Inject();
    EffectKindA0FromRest3f_Inject();
    EffectKind9eFromRest3f_Inject();
    EffectKind9cFromRest3f_Inject();
    EffectKind69FromRest3f_Inject();
    EffectKind5fFromRest3f_Inject();
    if (bof3::WantsShadow("rest_3b")) rest_3b::SelfTest();            // round 14 wave-three group R3B (0x4468B0..0x44CFF4: the result screen's EXP
                                // helpers, three percent clamps, the command menus' last steps and four
                                // dispatchers, 45 Effect_Handlers slots): its clones' calls re-aimed at the boss
                                // harness's recorders, four .data tables swapped for the fuzz only; after every
                                // harness's inject; ours that call it (battle_result, battle_e6, rest_2g) call it by
                                // the address it had; before FishingText_Arm; no module patches bytes inside its 64
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect_Inject();
    BattleItemCmd_Inject();
    Battle3_Inject();
    Rest4F_Inject();            // round 14 wave-four group R4F (0x460CB0..0x464B9E: the Config screen's machine and
                                // draws, WorldMap_ExitRecords, the states of effect kinds 2, 7, 8, 9 and 0xB and
                                // their draws): its clones' calls re-aimed at the scenario harness's recorders, five
                                // state tables swapped for the fuzz only; after every harness's inject and after
                                // ConfigText_Inject and MenuFrame_Inject, whose call sites and operands inside its
                                // Config draws ours reads in place (DIV-0011, DIV-0017, DIV-0026, DIV-0051); ours
                                // that call it (effect_1a, event_leader, rest_2f) call it by the address it had;
                                // before FishingText_Arm
    EffectWorldMapFromRest4f_Inject();
    EffectKind0fFromRest4f_Inject();
    EffectKind09FromRest4f_Inject();
    TitleFromRest4f_Inject();
    Rest4E_Inject();            // round 14 wave-four group R4E (0x45E870..0x460CAD: the community band's panels,
                                // name commits, track list, item screen and ranked lists): its clones' calls
                                // re-aimed at the scenario harness's recorders, seven .data tables swapped for the
                                // fuzz only; after every harness's inject; R4B and R4D call it by the address it
                                // had; before FishingText_Arm; no module patches bytes inside its 48 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    TitleFromRest4e_Inject();
    CommuMusic_Inject();
    CommuFromRest4e_Inject();
    Rest4D_Inject();            // round 14 wave-four group R4D (0x45C400..0x45E86E: the community's games 7
                                // and 8 - CommuDraw, CommuName - and the board R4C draws with): its clones' calls
                                // re-aimed at the scenario harness's recorders, eleven .data tables swapped for the
                                // fuzz only; after every harness's inject; before FishingText_Arm; no module patches
                                // bytes inside its 60 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    CommuName_Inject();
    CommuBoardFromRest4d_Inject();
    CommuFromRest4d_Inject();
    if (bof3::WantsShadow("rest_4a")) rest_4a::SelfTest();            // round 14 wave-four group R4A (0x452DD0..0x456D4F: six battle targeting helpers,
                                // Field_RunSlot and its CLUT copy, the community's simulation and its objects'
                                // poses, twelve Field_ObjectTriggers entries): its clones' calls re-aimed at the
                                // scenario harness's recorders; after every harness's inject; ours that call it
                                // (battle_sprites, battle_e1, battle_e2, frame_callees, area_w4d, mode_states) call
                                // it by the address it had; before FishingText_Arm; no module patches bytes inside
                                // its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Field2_Inject();
    DragonCmd_Inject();
    FieldTrigger01_Inject();
    CommuSim_Inject();
    if (bof3::WantsShadow("rest_4c")) rest_4c::SelfTest();            // round 14 wave-four group R4C (0x459EE0..0x45C3F3: the community band's two games
                                // with a stake, entries 0 and 1 of R4B's game table 0x652A84, their nine state tables
                                // and draw helpers, Commu_PushSubscreen): its clones' calls re-aimed at the scenario
                                // harness's recorders, its nine tables swapped for the fuzz only; after every
                                // harness's inject; before FishingText_Arm; no module patches bytes inside its 60
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp)
    CommuBoardFromRest4c_Inject();
    Rest4B_Inject();            // round 14 wave-four group R4B (0x456D50..0x459EDA: Field_ModeTailKinds 14, 21..26,
                                // 60, their states and the community board's states and draws): its clones' calls
                                // re-aimed at the scenario harness's recorders, twelve .data tables swapped for the
                                // fuzz only; after every harness's inject; before FishingText_Arm; no module patches
                                // bytes inside its 60 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    CommuTail_Inject();
    CommuBoardFromRest4b_Inject();
    SoundRest_Inject();         // the platform round's group PS (Sound_StopMusic, Sound_ResumeAll, Sound_MusicPlaying,
                                // Sound_PauseAll, SndBuf_SetVolume, Music_Halt, Music_Resume, SndStream_IsPlaying,
                                // Snd_Init): every call and jump of its clones re-aimed at a recorder, DirectSound
                                // faked; after every module that calls them (by name, or by the address they had);
                                // before FishingText_Arm; no module patches bytes inside its nine
    CoreSound_Inject();
    Shell_Inject();             // the platform round's group PW (the seven "Windows shell" starts: Input_Latch,
                                // Cfg_Load, Game_Init, Gfx_InitBufferBlock, Gfx_LinkOTags, Disc_Probe,
                                // Cfg_SetDefaultKeys): its clones' calls re-aimed at its own recorders; no module
                                // clones them; before FishingText_Arm (docs/shell.md)
    Magic080_Inject();
    File_Inject();
    Cfg_Inject();
    if (bof3::WantsShadow("psx_rest")) psx_rest::SelfTest();           // the platform round's group PL (docs/psx-rest.md: three libgpu setters, the texture
                                // window, SquareRoot0, ApplyMatrixSV and the teardown chain): every caller of the
                                // seventeen is ours and calls them by address, so after every module whose fuzz
                                // stands them in or runs Capcom's
    Music_Inject();
    Gte_Inject();
    Gpu2_Inject();
    Gpu_Inject();
    D3dRest_Inject();           // the platform round's group PH (docs/d3d-rest.md): the renderer's live remainder - five
                                // Direct3D handlers, POLY_FT3's two helpers, D3d_SetAlphaModulate, D3d_AfterDraw,
                                // Gfx_StoreImage; Gfx_DrawOTag reaches them by the addresses they had; its clones' calls
                                // re-aimed at its own recorders; before FishingText_Arm
    D3d_Inject();
    ModeRest_Inject();          // platform round step 2, group PM (GameMode_Handlers 3..6 and the steps of modes 3 and
                                // 5, 0x495BB0..0x496226, and Sound_MusicPlaying 0x587C20): its clones' calls re-aimed
                                // at the scenario harness's recorders, two .data tables swapped for the fuzz only;
                                // before FishingText_Arm; no module patches bytes inside its 14 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    TransitionFromModeRest_Inject();
    if (bof3::WantsShadow("game_last")) game_last::SelfTest();          // platform round step 3, group TWO (Item_UseFlags 0x591810, ItemTrade_Dispatch 0x593950; docs/game-last.md): no calls, one .data table swapped for the fuzz only; last of the takeovers
    ItemTrade_Inject();
    MenuChar_Inject();
    CrtRest_Inject();           // platform round step 3 (docs/crt-rest.md): the C runtime's entries the game calls -
                                // Rand, sprintf, strncpy, _stricmp, _findfirst / _findnext, the file layer - LAST of
                                // the takeovers: every module's fuzz before it ran Capcom's entries or its own stand-ins
                                // for them; it resets Rand's seed to the CRT's 1 after them all
    bof3x::input::sigma::Arm(); // DIV-0133 and DIV-0134 (docs/button-sigma.md): the Config as one permutation, words 7 / 8
                                // as Speak's and Action's, the starting Config by source - after every module's self-test,
                                // which all compared Capcom's stores at Boot_Task, LoadMenu_Apply and the rebind
    ConfigText_ArmJapanese();   // DIV-0051's 2026-10-21 amendment: the Config panel's ja-JP icons, after rest_4f's fuzz
    ButtonText_Arm();           // DIV-0140 (docs/button-dialogue.md 4): the dialogue's button mentions drawn as
                                // Sigma's button - after sigma::Arm, and after every module's self-test, which all
                                // compared Capcom's text draws (text_draw's, msgbox's)
    if (bof3::WantsShadow("button_tests"))   // group HT (docs/button-tests.md 4): the hard-coded tests' masks and four
        bof3x::input::button_tests::Check(); // of ours under stand-in Config words - after Arm, for the live table
    shoulder_glyphs::Arm();     // DIV-0138 (docs/button-shoulder-corner.md): the item lists' L1 / R1 corner as sigma has
                                // it - after sigma::Arm and after every module's self-test (menu_windows' compared the original's
                                // Menu_DrawPiece)
    world_map_legend::Arm();    // button round's DIV-0135 (world_map.h): the world map's legend on the port's own
                                // page - after every module's self-test, whose frame-draw fuzzes all ran the original's
    AreaKeepStoppingNone_Arm(); // DIV-0143 (issue #88): an area change keeping a track that is fading to a stop
                                // keeps none - after every module's self-test, which all ran Capcom's keep
    FishingEmpty_Arm();         // DIV-0144 (D279): the fishing banner declines an empty line and a one-byte code below
                                // 0x26 - after every module's self-test, which all compared Capcom's draws; before
                                // FishingText_Arm, so its self-test (fishing_empty) reads the original's label
    FishingText_Arm();        // DIV-0069: the fishing text's Latin layout - after every module's self-test, which
                                // all compared Capcom's (effect_1a's and effect_1b's draws read it)
    layering::Arm();            // DIV-0071: the floor under a sprite drawn before it (BOF3X_LAYERING) - after every
                                // module's self-test, which all compared the original's order (layering.h)
    Widescreen_ArmFills();     // DIV-0041 section 3c: the full-frame fills widen from here - after every module's
                                // self-test, which all compared the original's (0, 0) 320 x 240 (widescreen.h)
    wide_staging::Inject();     // DIV-0122 / 0123 / 0124 (docs/wide-staging.md): the wide modes' re-staged walks, case zero and
                                // the 21:9 hide - armed after every module's self-test, which all ran the original's
                                // walks and draws (MoveScript_Step's, EventScript_Op's and Sprite_Draw's fuzzes)
    if (WantsShadow("gapwatch"))   // BOF3X_GAPWATCH's self-test (gapwatch.h): a scratch page, no game state; the watch
        GapWatch_SelfTest();       // itself arms in Engine_Main, after InputScript_Start
    band_census::Inject();      // BOF3X_BANDLOG, an instrument (docs/band-census.md): armed after every module's self-test,
                                // whose stand-in sprites Sprite_UpdateScreen must not hand it; reads Widescreen_Live
    pillarbox::Inject();        // DIV-0041 (PB, docs/narrow-pillarbox.md): the narrow areas' bars, drawn at the present - after
                                // Widescreen_ArmFills, before which they draw nothing; the rule's self-test (BOF3X_SHADOW=pillarbox)
    d3d_lines::Arm();           // DIV-0079: the six LINE handlers draw quads of the scale's width (BOF3X_LINES) - after
                                // every module's self-test, which all compared the original's line strip (d3d_lines.h)
    map_layers::ArmSideDup();   // DIV-0085: a side face gained at run time takes the cell's one side word (BOF3X_SIDE_DUP;
                                // known-defects.md D239) - after every module's self-test, which all compared Capcom's read
    map_rim::Arm();             // DIV-0041 (wide-ring, docs/wide-ring.md): the grown view ring's rim, and
                                // BOF3X_SHADOW=map_rim's model test - after every module's self-test, which all ran
                                // Capcom's 28 x 56 alone; before DrawPool_Grow and GfxSnapshot_Inject
    music_loops::Arm();         // the measured music loops (BOF3X_MUSIC_LOOPS; docs/bgm-comparison.md section 12) -
                                // after every module's self-test, which all compared the original's rewind
    dat_cache::Arm();           // DIV-0089: DAT\ and SND\ from the cache where it has them (BOF3X_CACHE,
                                // BOF3X_CACHE_DATA) - after every module's self-test, which all read the install's
    mp3dec::Arm();              // DIV-0091: the MP3s through minimp3 (BOF3X_MP3) - after every module's self-test,
                                // which all compared Capcom's decoder (audio/mp3.h)
    music_seq::Arm();           // DIV-0087: the cache's songs through the sequencer (BOF3X_CACHE, BOF3X_MUSIC) - after
                                // every module's self-test, which all compared the MP3 path
    sound_sp::Arm();            // DIV-0170..0173 (the sound-layer round's SP, sound_sp.h): its self-test
                                // (BOF3X_SHADOW=sound_sp), then the three switches - after every module's self-test,
                                // which all compared the PC's B8, Sound_PlayById, screen update and level calls
    DrawPool_Grow();            // DIV-0062: the draw-item pool doubled - LAST, after every module's self-test,
                                // which all compared the original's arrays (draw_pool.h)
    GfxSnapshot_Inject();       // the quicksave round's SP-G (render/gfx_snapshot.h): the graphics participant and
                                // its self-test - before Snapshot_Inject, beside it, after every module's self-test
    Snapshot_Inject();          // the quicksave round's SN (docs/quicksave.md): the snapshot core's self-test
                                // (BOF3X_SHADOW=snapshot) - after DrawPool_Grow, which places the hosted pool it copies
    QuickMenu_Inject();         // the quicksave round's QM: the lists' and the cursor's self-test, then draft B's
                                // split save list (SaveLists) - after every module's self-test, which compared the original's
    MountainPasses_Arm();       // DIV-0191 (game/event_script.h): the placement hook, the step hook and the bounded
                                // plate searches - after every module's self-test, which compared the unarmed code
    DesertFaerie_Arm();         // DIV-0204 (game/event_script.h): the desert map's faerie cell, opt-in - after
                                // every module's self-test, which compared the placement runner without it
    InjectReport();
}

}  // namespace bof3
