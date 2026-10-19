#!/usr/bin/env python
"""The quicksave census (docs/quicksave-census.md): every mutable static of
bof3x.dll's own code, read from the build's object files, each put in a class
by the rules below. Read-only: it runs llvm-nm and llvm-cxxfilt and writes
nothing.

    python tools/quicksave_census.py [--build build] summary
    python tools/quicksave_census.py [--build build] table      # the markdown rows, tooling by file
    python tools/quicksave_census.py [--build build] all        # one TSV row per object, tooling too
    python tools/quicksave_census.py [--build build] check      # exit 1 if any object has no rule

The sweep: every `.obj` of the bof3x target under src/ (game, render, audio,
input, hook, common), and in each every defined symbol of COFF type b/B/d/D
(.bss and .data: namespace-scope objects, anonymous-namespace ones, class
statics, function-local statics and their guard variables). Read-only data
(r/R) is not mutable and is not listed. Thread-local storage: bof3x.dll's .tls
is 0x10 bytes of the toolchain's own and no source of ours says thread_local.
A static the optimiser folded away holds nothing across frames and is not
seen - correctly. The DLL's other data symbols (SDL3, libc++, the mingw
runtime) are counted, not classed (the doc's section 1).

A new static with no rule makes `check` fail: the census is kept by adding
its row here, with its class, the participant that must carry it, and how it
is rebuilt - never by widening a pattern to swallow it unread.
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PF, CF, GS, TL = 'per-frame', 'configuration', 'game state', 'tooling'

# (file regex, name regex, class, participant, rebuild source / note). First
# match wins. Names are demangled with "(anonymous namespace)::" removed and a
# trailing " (.N)" (a split aggregate) dropped. Participants: SP-A audio,
# SP-G graphics, QM the quick menu, ST ours' statics (proposed here: a byte
# copy of named statics of bof3x.dll plus rebuild hooks, owned by SN), "-" none.
RULES = [
    # --- whole files that are tooling -----------------------------------------
    (r'_fuzz\.cpp$', r'', TL, '-', 'BOF3X_SHADOW self-test fixture; runs before play'),
    (r'(_harness\w*|harness_eh)\.cpp$', r'', TL, '-', 'scenario / boss / area / magic harness (BOF3X_SHADOW); before play'),
    (r'band_census\.cpp$', r'', TL, '-', 'BOF3X_BAND_CENSUS instrument (the spec\'s "band census")'),
    (r'button_tests\.cpp$', r'', TL, '-', 'the button round\'s self-test'),
    (r'hook/(calltrace|code_bytes|draw_order)\.cpp$', r'', TL, '-', 'instrument (call trace / operand store / draw-order log)'),
    (r'hook/(crash|log)\.cpp$', r'', TL, '-', 'crash reporter / log writer and their threads; touch no span byte'),
    (r'hook/gapwatch\.cpp$', r'', TL, '-', 'BOF3X_GAPWATCH read watch, off by default (section 2a)'),
    (r'hook/(statehash|stackwatch|stallwatch|wav_dump)\.cpp$', r'', TL, '-', 'instrument; its own counters keep running across a restore'),
    (r'hook/input_script\.cpp$', r'', TL, '-', 'recipe player/recorder: counts its own frames, not Frame_Counter (RC: F7 does not rewind a recipe)'),
    (r'', r'^guard variable for ', CF, '-', 'a local static\'s once-flag'),
    # --- self-test scaffolding inside game files -------------------------------
    (r'', r'(SelfTest\w*|Fuzz\w*|LiveRuns|LiveDraw|\bLive|Capture\w*|EngineTest|CheckRowDoneReset|RunOne|\bRun|LtRun|RtStream)\(', TL, '-', 'a self-test\'s local buffer'),
    (r'layering\.cpp$', r'::g_log$', CF, '-', 'BOF3X_LAYERING_LOG switch'),
    (r'', r'::(g_rng|g_log|g_log_n|g_bad|g_rounds|their_\w+|our_\w+|\w+_theirs|g_theirs|\w+_in|g_input|saved\w*|g_test_\w+|g_\w+_clone|g_fake\w*|g_lt_\w+|g_rt_cursor|g_fpf_\w+|g_checks)$', TL, '-', 'self-test state'),
    (r'', r'::(logged|seen|said|bof3_seen_|full_logged|seen_high|g_\w+_logged|g_textlog\w*|reports|last|want|asked)$', TL, '-', 'log-once / log switch'),
    (r'fmv_play\.cpp$', r'LogNotOpened.*::count$', TL, '-', 'log count'),
    (r'', r'^(\w+::)*(g|kOriginals|g_io|g_c|SoundRest_g)$', CF, '-', 'callee table, bound at inject'),
    # --- the self-tests' fixtures by file (declared beside SelfTest, read by it alone) ---
    (r'(area_slope|draw_emit|draw_layers|draw_pass|draw_pool|field_blocked|field_frame|field_objects|frame_callees|'
     r'kind2_object|map_cells|map_view|mode_tasks|move_groups|move_script|object_kinds|sprite_draw|sprite_find|'
     r'sprite_order|sprite_screen|title_states|gfx_clut|gfx_flush|gfx_unpack|gfx_vram_ops|psx_gpu|text_draw)\.cpp$',
     r'::(g_seed|g_object|g_sprite|g_descriptor|g_scripts|g_scripts18|g_script|g_field|g_state|g_handle_buffer|g_offset|'
     r'g_vectors|g_other|g_after_step|g_current_object|g_hit|g_member|g_pace_stubs|g_fade_table|g_ops_seen|g_label_target|'
     r'g_skip|g_depths|g_depth_n|g_sprites|g_strip_\w+|g_prim|g_sprite_out|g_cover|g_jump|g_sleeps|g_sleep_limit|'
     r'g_load_calls|g_load_zeros|g_records|g_band_fill|g_objects|g_prims|g_tails|g_draw|g_open|g_add_records|g_stub_seed|'
     r'g_open_\w+|g_runs|g_run_count|g_record|g_rand|g_live|g_\w+_live|g_live_\w+|g_counts|g_canvas|g_draw_columns|'
     r'g_flag_columns|g_upload|g_buffers|g_row_buffers|g_literals|g_overlapping|g_packed|g_matches|g_low_items_test)$',
     TL, '-', 'self-test fixture or live-check instrument (BOF3X_SHADOW)'),
    (r'gfx_texcache\.cpp$', r'::(g_plan|g_shadow|g_release_n|g_their_log|g_release_log)$', TL, '-', 'texture-cache live shadow check'),
    (r'psx_gte_float\.cpp$', r'::g_live$', TL, '-', 'live shadow counts'),
    (r'msgbox\.cpp$', r'::g_record(_n|_max)?$', TL, '-', 'the message box\'s draw recorder (self-test)'),
    (r'scenario_harness_ekh\.cpp$', r'', TL, '-', 'harness'),
    (r'music_seq\.cpp$', r'::(g_files|g_names|g_buffer|g_levels|g_vtable|g_capture|g_handles|g_decoders_opened)$', TL, '-', 'the synth\'s self-test seam'),
    (r'save_menu\.cpp$', r'::(g_sound_banks|g_sound_opens)$', TL, '-', 'self-test seam'),
    (r'dat_cache\.cpp$', r'::(g_saved|g_topen|g_tfiles|g_topened)$', TL, '-', 'self-test seam'),
    (r'dat_load\.cpp$', r'::(g_walked|g_datlog|g_datlog_index|g_datlog_caller)$|DatLog\(', TL, '-', 'BOF3X_DATLOG / self-test seam'),
    (r'wide_rand\.cpp$', r'::(g_control|g_saved|g_capcom_n|g_capcom_log|g_draw_only_n|g_rising_cells|g_band_cells|g_cell|g_seen|g_seen_band)$', TL, '-', 'the random split\'s self-test stand-ins'),
    (r'wide_staging\.cpp$', r'::(g_call_count|g_cz_a|g_cz_b|g_cz_have|g_kind|g_move|g_calls|g_quiet|g_live)$', TL, '-', 'the staging engine\'s self-test and live census'),
    (r'shoulder_glyphs\.cpp$', r'::g_rng$', TL, '-', 'self-test'),
    (r'crt_rest\.cpp$', r'::(g_counting|g_count|g_original_copy)$', TL, '-', 'BOF3X_RANDLOG counter'),
    (r'effect_1b\.cpp$', r'::g_glyph_calls$', TL, '-', 'DIV-0144 self-test counter'),
    (r'sprite_find\.cpp$', r'::g_records$', TL, '-', 'self-test'),
    (r'world_map\.cpp$', r'::g_unknown_count$', TL, '-', 'log count'),
    (r'fb_grab\.cpp$', r'', TL, '-', 'DIV-0125 frame grab (BOF3X_SHOT_DIR); the registry lives in render_shim'),
    (r'render_shim\.cpp$', r'::g_grabs$', TL, '-', 'frame grab registry (BOF3X_SHOT_DIR)'),
    (r'win_main\.cpp$', r'::(g_real_\w+|g_fps_log|g_speed_scripted)$', TL, '-', 'BOF3X_EXITTRACE / FPS log / BOF3X_SPEED (scripted runs)'),
    (r'inventory_ops\.cpp$', r'::g_on_screen_log$', TL, '-', 'D56 log switch'),

    # --- GAME STATE ------------------------------------------------------------
    (r'crt_rest\.cpp$', r'::g_seed$', GS, 'ST', 'none - Rand\'s seed (MSVC holdrand) is ours since crt-rest 2.1: 4 bytes, copied'),
    (r'draw_pool\.cpp$', r'DrawPool_Grow\(\)::free_list$', GS, 'ST', 'none - DIV-0062\'s 2,048-entry free ring; DrawItemPool_Top and the items (.pool) are in the span: 4 KB, copied with them'),
    (r'msg_pool\.cpp$', r'MsgPool_Relocate\(\)::pool$', GS, 'ST', 'the area\'s DAT (the 0x4000 chunk) - copied, 32 KB; under a language overlay only (DIV-0007)'),
    (r'sound\.cpp$', r'::g_loop$', GS, 'SP-A', 'the measured loop\'s row and position: reopen + ResumeSkip'),
    (r'sound\.cpp$', r'::g_ring$', GS, 'SP-A', 'DIV-0092 ring bookkeeping: rebuilt by Music_Start + ResumeSkip'),
    (r'sound\.cpp$', r'::g_resume$', GS, 'SP-A', 'DIV-0092 remembered / pending area-track position: saved as numbers'),
    (r'sound\.cpp$', r'sound::g_unducked$', GS, 'SP-A', 'DIV-0132 duck outstanding: saved'),
    (r'sound\.cpp$', r'sound::(g_fade_frame|g_stop_unseen)$', GS, 'SP-A', 'DIV-0028 fade state beside Music_FadeCount/Step (span): saved'),
    (r'music_seq\.cpp$', r'::g_s$', GS, 'SP-A', 'DIV-0087 song/bank/synth (heap): LoadFile(track) + FastForward(sample)'),
    (r'save_menu\.cpp$', r'::g_stream_missing$', GS, 'SP-A', 'DIV-0129: Sound_StreamDone answers 1 while set - logic-visible; saved'),
    (r'save_menu\.cpp$', r'::g_stream_started$', CF, '-', 'BOF3X_SPEED\'s hold only ("no game state", its comment); ST may clear it'),
    (r'map_layers\.cpp$', r'::g_file_sides$', GS, 'SP-G', 'DIV-0085: side faces as the area file loaded them - not rebuildable from the patched arena; copied, 64 KB'),
    (r'widescreen\.cpp$', r'::g_split$', GS, 'SP-G', 'DIV-0041 faster slide: 22 window records\' split; copied (wide only)'),
    (r'wide_rand\.cpp$', r'::g_seed$', GS, 'SP-G', 'DIV-0041 random split\'s draw-only generator: copied (wide only; picture)'),
    (r'wide_rand\.cpp$', r'::(g_noted|g_original|g_written)$', GS, 'SP-G', 'the last inset NoteInset saw; copied (wide only; picture)'),
    (r'wide_rand\.cpp$', r'::g_band$', PF, '-', 'set and cleared around one DrawLayer_Open'),
    (r'input/sigma\.cpp$', r'::g_release_words$', GS, 'ST', 'DIV-0133/0134: whose words stand (release start vs a save\'s): saved, 1 byte'),
    (r'input/sigma\.cpp$', r'::g_table$', GS, 'ST', 'derived from the Config words in the span: rebuilt by Follow after the span'),

    # --- per-frame / scratch ---------------------------------------------------
    (r'layering\.cpp$', r'::(g_runs|g_runs_now|g_runs_before|g_pass)$', PF, 'SP-G', 'one pass of history (runs stamped g_pass - 1): SP-G invalidates (g_pass += 2) so the first pass after a restore uses the proximity test'),
    (r'layering\.cpp$', r'::(g_cell_of|g_moved|g_moved_n|g_pending|g_pending_n|g_run|g_in_run|g_why)$', PF, '-', 'reset by Defer each pass'),
    (r'field_frame\.cpp$', r'::(g_band|g_band_count|g_band_added)$', PF, '-', 'band sprites noted and put back within one Field_DrawFrame'),
    (r'world_map\.cpp$', r'::g_standin$', PF, '-', 'set by Begin each frame'),
    (r'world_map\.cpp$', r'KeepDimCluts\(\)::written$', CF, '-', 'once a process (log)'),
    (r'dat_load\.cpp$', r'WalkOverlay\(.*::cache$', PF, '-', 'path scratch inside one call'),
    (r'dat_cache\.cpp$', r'WalkShipped\(.*::(zh_path|base_path)$', PF, '-', 'path scratch inside one call'),
    (r'sound\.cpp$', r'ResumeSkip\(.*::scratch$', PF, '-', 'decode scratch inside one call'),
    (r'field_e2\.cpp$', r'ConfirmLayout\(.*::line$', PF, '-', 'text scratch inside one call'),
    (r'map_layers\.cpp$', r'SnapshotSides\(\)::(used|next_used)$', PF, '-', 'scratch of one SnapshotSides'),
    (r'audio/mp3\.cpp$', r'DecodeAll\(.*::(out|last)$', TL, '-', 'DecodeAll (the decoder\'s self-test / fuzz path)'),
    (r'music_seq\.cpp$', r'::g_parsing$', PF, '-', 'the file being parsed, null between calls'),
    (r'field_blocked\.cpp$', r'BlockingNibbles\(\)::mask$', CF, '-', 'a table built on first use'),

    # --- configuration: inject-time switches, operands, tables -----------------
    (r'(battle_draw|battle_e7|battle_result|battle_win_states|d3d_lines|d3d_rest|effect_1a|effect_1e|effect_1g|effect_gte|'
     r'event_leader|field_e2|field_o|field_s|glyph_draw|inventory_ops|menu_draw_helpers|menu_frame|menu_lists|menu_verbs|'
     r'menu_windows|mode_flow|msgbox|music_loops|psp_code|rest_2c|rest_2f|rest_2h|rest_3e|rest_4d|shell|shop_states2|'
     r'sprite_draw|text_draw|text_immediate|title_menu|window_task|world_map|yes_no_layout|title_states|cheats)\.cpp$',
     r'', CF, '-', 'a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV)'),
    (r'(area_w4a|magic_s02|magic_s20|psx_gte|psx_gte_float|psx_gte_matrix|psx_gte_transform)\.cpp$', r'_Inject\(\)::', CF, '-', 'inject table'),
    (r'd3d_list\.cpp$', r'Operands\(\)::', CF, '-', 'operand values'),
    (r'config_text\.cpp$', r'', CF, '-', 'DIV-0015 Config screen text and operands (language overlay)'),
    (r'(battle_text|labels|fishing_text|pause_text)\.cpp$', r'', CF, '-', 'language overlay text (DIV-0038/0064/...); the span\'s pointer slots name it (section 5)'),
    (r'text_advance\.cpp$', r'', CF, '-', 'DIV-0006 advance table (heap block of ours), from the overlay'),
    (r'text_pairs\.cpp$', r'', CF, '-', 'pair-code table (heap block of ours), from the overlay'),
    (r'text_draw\.cpp$', r'', CF, '-', 'glyph count / limit (DIV-0016), button-text switch'),
    (r'msg_pool\.cpp$', r'::g_pool$', CF, '-', 'where Msg_SystemPtr reads (arena or the relocated pool), set at start-up'),
    (r'draw_pool\.cpp$', r'', CF, '-', 'DIV-0062 operands and the pool\'s place (.pool in the exe host)'),
    (r'dat_load\.cpp$', r'', CF, '-', 'install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up'),
    (r'dat_cache\.cpp$', r'', CF, '-', 'cache constants'),
    (r'map_layers\.cpp$', r'::(g_side_dup|g_side_zero_release)$', CF, '-', 'DIV-0085 switches'),
    (r'(widescreen|wide_staging|wide_rand|shoulder_glyphs|sprt_draw)\.cpp$', r'', CF, '-', 'wide-mode / glyph-sheet set-up, fixed at arm'),
    (r'layering\.cpp$', r'', CF, '-', 'DIV-0071 mode and tuning (BOF3X_LAYERING*)'),
    (r'sound\.cpp$', r'', CF, '-', 'switch / log flag'),
    (r'crt_rest\.cpp$', r'RandSeedCell', CF, '-', 'which seed cell (ours or the CRT\'s)'),
    (r'game_clock\.cpp$', r'', CF, '-', 'DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept'),
    (r'display_setup\.cpp$', r'', CF, '-', 'platform: backend up, scale k (DIV-0036)'),
    (r'win_main\.cpp$', r'', CF, '-', 'window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player\'s / the process\'s, kept'),
    (r'pad_read\.cpp$', r'', CF, '-', 'pad layout and map (ini / BOF3X_PAD)'),
    (r'input/(pad_sdl|bindings)\.cpp$', r'', CF, '-', 'platform: the SDL gamepad handle, layout, default keys'),
    (r'input/sigma\.cpp$', r'', CF, '-', 'DIV-0133/0134 switches and the install\'s release, fixed at Arm'),
    (r'audio/(song|spu)\.cpp$', r'::g_abort_hook$', CF, '-', 'abort hook'),
    (r'audio/seq\.cpp$', r'Pitches\(\)::t$', CF, '-', 'pitch table, built once'),
    (r'audio/mp3\.cpp$', r'::g_stack$', CF, '-', 'platform: the decoder\'s 256 KB private stack (VirtualAlloc), its contents dead between calls'),
    (r'audio/mp3\.cpp$', r'::(g_active|g_have_simd)$|have_simd', CF, '-', 'switch / CPU probe'),
    (r'render/render_d3d11\.cpp$', r'::(g_opt|g_look|g_pixel_offset)$', CF, '-', 'renderer options (ini)'),
    (r'render/render_d3d11\.cpp$', r'::g_rescale_hook$', CF, '-', 'hook pointer'),
    (r'render/render_d3d11\.cpp$', r'', CF, '-', 'platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces'),
    (r'render/satpixie\.cpp$', r'::g_flip$', PF, '-', 'blur ping-pong'),
    (r'render/satpixie\.cpp$', r'', CF, '-', 'platform: DIV-0043 look\'s textures and shaders'),
    (r'render/render_shim\.cpp$', r'::(g_surfaces|g_n_surfaces)$', CF, 'SP-G', 'platform: every surface made (process heap); the span\'s texture caches name them - SP-G releases the caches\' surfaces and clears the records'),
    (r'render/render_shim\.cpp$', r'::g_state$', CF, 'SP-G', 'platform: the pipe state the device holds; the span D3d_ShadeModeCache / Gfx_RenderFlags / D3d_BackMaterialHandle cache it - SP-G pushes the restored caches to it or marks them unknown'),
    (r'render/render_shim\.cpp$', r'', CF, 'SP-G', 'platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5)'),
    (r'hook/exe_icon\.cpp$', r'', CF, '-', 'platform: DIV-0095 icon, made once from the VRAM shadow'),
    (r'hook/(detour|exe_host|exe_image|exe_own|host)\.cpp$', r'', CF, '-', 'inject / host tables, fixed at start-up'),
]


def tool(name):
    p = shutil.which(name) or shutil.which(name + '.exe')
    if not p:
        sys.exit(f'{name} not on PATH (the llvm-mingw toolchain, docs/SCAFFOLDING.md 4)')
    return p


def sweep(build):
    objs = sorted(glob.glob(os.path.join(build, 'CMakeFiles', 'bof3x.dir', 'src', '**', '*.obj'), recursive=True))
    if not objs:
        sys.exit(f'no objects under {build}/CMakeFiles/bof3x.dir/src: build first')
    nm = tool('llvm-nm')
    rows = []
    for f in objs:
        rel = os.path.relpath(f, os.path.join(build, 'CMakeFiles', 'bof3x.dir')).replace('\\', '/')[:-4]
        out = subprocess.run([nm, '--defined-only', f], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            p = line.split()
            if len(p) == 3 and p[1] in 'bBdD':
                rows.append([rel, p[2]])
    # i386 COFF names carry one leading underscore before the C++ mangling's own
    dem = subprocess.run([tool('llvm-cxxfilt')], input='\n'.join(n[1:] if n.startswith('_') else n for _, n in rows),
                         capture_output=True, text=True, check=True).stdout.split('\n')
    out = []
    for (rel, raw), d in zip(rows, dem):
        name = d.replace('(anonymous namespace)::', '')
        name = re.sub(r' \(\.\d+\)$', '', name)
        out.append((rel, name))
    # a split aggregate's pieces are one object
    return sorted(set(out)), len(objs)


def classify(rel, name):
    for frx, nrx, cls, part, note in RULES:
        # a top-level anonymous namespace's name has no qualifier: matched as "::name" too
        if re.search(frx, rel) and (re.search(nrx, name) or re.search(nrx, '::' + name)):
            return cls, part, note
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--build', default=os.path.join(ROOT, 'build'))
    ap.add_argument('what', choices=['summary', 'table', 'all', 'check'], nargs='?', default='summary')
    a = ap.parse_args()
    objs, nobj = sweep(a.build)
    done, missing = [], []
    for rel, name in objs:
        c = classify(rel, name)
        (done if c else missing).append((rel, name) + (c or ()))
    if a.what == 'check' or missing:
        for rel, name in missing:
            print(f'NO RULE\t{rel}\t{name}')
        if missing:
            print(f'{len(missing)} of {len(objs)} objects have no rule', file=sys.stderr)
            return 1
        if a.what == 'check':
            print(f'{len(objs)} objects in {nobj} object files, every one classed')
            return 0
    if a.what == 'summary':
        from collections import Counter
        c = Counter(r[2] for r in done)
        p = Counter((r[2], r[3]) for r in done)
        print(f'{nobj} object files, {len(done)} mutable objects')
        for k, v in sorted(c.items()):
            print(f'  {k:14} {v:5}')
        for (k, part), v in sorted(p.items()):
            if part != '-':
                print(f'  {k:14} {part:5} {v:4}')
        return 0
    if a.what == 'all':
        for r in done:
            print('\t'.join(r))
        return 0
    # table: every object that is not tooling, one row; tooling by file
    print('| Module | Object | Class | Participant | Rebuild source / note |')
    print('|---|---|---|---|---|')
    for rel, name, cls, part, note in done:
        if cls == TL:
            continue
        print(f'| `{rel.replace("src/", "")}` | `{name}` | {cls} | {part} | {note} |')
    print()
    from collections import Counter
    t = Counter(rel for rel, name, cls, *_ in done if cls == TL)
    print('| Module (tooling only) | Objects |')
    print('|---|--:|')
    for rel, n in sorted(t.items()):
        print(f'| `{rel.replace("src/", "")}` | {n} |')
    return 0


if __name__ == '__main__':
    sys.exit(main())
