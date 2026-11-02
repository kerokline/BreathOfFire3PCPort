# The asm policy for the 64-bit build, settled by a build (XA3)

**Status:** STABLE (measured 2026-11-01 on `x64/round` `21d06502`, the
`x64/xa3` worktree; a record of what the `x86_64` driver makes of every
inline-asm block in the live source, and stage B1's work list. Source
unchanged by this doc.)

Stage A, group XA3 of [`x64-build-round.md`](x64-build-round.md) section 3.
The round doc expected the 104 x87 and 23 control-word blocks to assemble
under `x86_64-w64-mingw32-clang++` unchanged and the rest not, by a reading
of mnemonics. This doc replaces the expectation with a build: every live
file that holds asm compiled with the `x86_64` driver and the project's real
compile command, each failing block named with its diagnostic, each
surviving block checked for whether what it assembles to is also *right* on
x86-64 (an instruction that assembles can still read `4(%esp)` for an
argument the x86-64 caller put in `ecx`). Section 5 answers which of the 27
live naked functions the exe host can reach at all; section 8 is B1's list.

## 0. The answer in six lines

1. **All 127 x87 and control-word blocks assemble unchanged** and are right
   as they stand: memory operands, `st`, the control words. They stay asm
   (section 3's rule, the round doc's).
2. **15 of the 30 stack blocks fail to assemble**, every one on `push` /
   `pop` of a 32-bit register or a `jmp *%e..`, illegal in 64-bit mode. The
   other 15 assemble but 12 of them are wrong on x86-64 (the i686 stack
   argument, a 32-bit `esp`, a struct offset that doubles).
3. **The 6 `fs:` reads assemble and read the wrong segment**: on x86-64
   Windows the TEB is at `gs:`, with `StackBase` at `+8`, `StackLimit` at
   `+0x10`, the PEB at `+0x60` and its `ImageBaseAddress` at `+0x10`.
4. **The 11 "other" blocks assemble and are right**, the privileged `in`
   included.
5. **Every one of the 27 naked functions has only our callers**, in both
   hosts: the twelve `[[func]]` rows without `impl` (section 5) call none of
   them. Three are unreachable in the exe host (the fuzz's own helpers,
   behind `BOF3X_SHADOW`, which the exe host refuses).
6. **The compile found a wider defect first**: 109 `GameData` members are
   pointer-typed in the generated header, so `sizeof(GameData)` is 4,640
   bytes too large on x86-64 and no game file compiles until B3 narrows
   them (section 7). The round doc's "no pointer-typed member" was wrong.

## 1. Method

The commands, all run in the worktree `C:/Users/kerok/bof3x-runs/x641101/xa3`
(scratch scripts under `scratch/`, not in git; what they do is written here
so XA1's `tools/width_census.py` can carry the same rules):

- **The blocks.** A classifier over every `asm` / `__asm__` statement in
  `src/**/*.cpp` and `*.h` whose file name lacks `fuzz`, comments stripped
  first, the template (the string literals before the first operand colon)
  classified by the round doc's rules: *stack* if the text has `push`,
  `pop`, `call`, `jmp`, `ret`, a `j` branch, `esp` or `ebp`; else *fs* if it
  reads `fs:`; else *x87* if any mnemonic is an x87 operation other than
  `fnstcw` / `fldcw` / `fnstsw`; else *cw* if every mnemonic is `fnstcw`,
  `fldcw`, `stmxcsr` or `ldmxcsr`; else *other*. An `__asm__("_Name")` after
  a declaration's `)` is an alias, not a block. Result: **174 blocks in 70
  files: 104 x87 in 36 files (32 hold nothing else), 23 cw in 11, 30 stack
  in 15, 6 fs in 3, 11 other in 10; 6 aliases** - the round doc's section 0
  figures exactly.
- **The compile.** `cmake --preset i686 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`,
  then `cmake --build build --target bof3x_symbols bof3x_rebase bof3x_suites`
  for the generated headers; for each of the 69 `.cpp` the file's own entry
  of `build/compile_commands.json` with the driver swapped for
  `x86_64-w64-mingw32-clang++` (clang 22.1.8, `x86_64-w64-windows-gnu`),
  `-ferror-limit=0` added, the object redirected under `scratch/obj/`;
  `image_base.h` is judged by its 69 includers. The i686 flags are kept as
  they are (`-O2 -g -DNDEBUG -std=c++20 -Wall -Wextra -Werror=return-type
  -falign-functions=16 -fno-strict-aliasing`, the DLL's includes).
- **The header patch the compile needed.** The first run stopped in every
  game file at the generated header (section 7): 2,969 errors each, none
  about asm, and clang runs no code generation after a front-end error, so
  no asm was judged. The second run adds `-Iscratch/gen` ahead of
  `build/gen`, a copy of `symbols.gen.h` with each of the 109 `using
  X_type = T *;` lines rewritten `std::uint32_t` (B3's narrowing, applied to
  the copy alone). That is the one "stub" in the method, and it is the
  shape the real header takes in B3.
- **Four files still stop in the front end** for reasons of their own
  (section 7): `mp3.cpp`, `sound_rest.cpp`, `gapwatch.cpp`,
  `stackwatch.cpp`. Their 12 blocks are judged by `scratch/iso_blocks.cpp`,
  each block copied with operands of the same C++ types, compiled with both
  drivers: the i686 compile passes (the copies are faithful), the x86_64
  compile is the verdict.
- **Right, not only assembled.** The objects of the files that compiled were
  read with `llvm-nm -u` and `llvm-objdump -r` (the undefined symbols and
  the relocation kinds the asm left) and `llvm-objdump -d` (the x87
  mnemonics present). What each surviving stack, fs and other block does on
  x86-64 is a reading of its text against the x86-64 ABI, written down per
  block in section 4.

## 2. Per file

`x86_64 -c` is the file's result after the header patch. "Blocks that
fail" are blocks with at least one assembler diagnostic, the count of
diagnosed lines in parentheses. The table is generated from the classifier's
TSV joined with the compile logs (`scratch/table.py`).

| File | Blocks | x87 | cw | stack | fs | other | `x86_64 -c` | Blocks that fail |
|---|---|---|---|---|---|---|---|---|
| `audio/mp3.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | front end stops (section 7); blocks judged in isolation | 174 fails (2) |
| `common/image_base.h` | 1 | 0 | 0 | 0 | 1 | 0 | assembles (in every includer) | - |
| `game/area_slope.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 5 diagnosed asm lines | 153 (5) |
| `game/area_w1a.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/area_w3g.cpp` | 2 | 2 | 0 | 0 | 0 | 0 | assembles | - |
| `game/area_w4e.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/battle_draw.cpp` | 2 | 2 | 0 | 0 | 0 | 0 | assembles | - |
| `game/battle_items.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/battle_misc.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | assembles | - |
| `game/boss_harness.cpp` | 4 | 0 | 0 | 0 | 4 | 0 | assembles | - |
| `game/d3d_draw.cpp` | 8 | 8 | 0 | 0 | 0 | 0 | assembles | - |
| `game/d3d_rest.cpp` | 8 | 8 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_1a.cpp` | 7 | 7 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_2a.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_2c.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_2e.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_2f.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_3a.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_3b.cpp` | 6 | 6 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_3c.cpp` | 7 | 7 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_3d.cpp` | 8 | 7 | 1 | 0 | 0 | 0 | assembles | - |
| `game/effect_4b.cpp` | 6 | 6 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_4c.cpp` | 4 | 4 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_4d.cpp` | 2 | 2 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_4e.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_4f.cpp` | 3 | 3 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_5a.cpp` | 2 | 2 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_5b.cpp` | 4 | 4 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_5c.cpp` | 2 | 2 | 0 | 0 | 0 | 0 | assembles | - |
| `game/effect_5e.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/event_objs.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/event_ops.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | assembles | - |
| `game/event_script.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/field_e2.cpp` | 5 | 4 | 0 | 1 | 0 | 0 | assembles | - |
| `game/field_misc.cpp` | 4 | 2 | 0 | 2 | 0 | 0 | assembles | - |
| `game/field_modes.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | assembles | - |
| `game/frame_callees.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/kind2_object.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/magic_s28.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | assembles | - |
| `game/magic_s32.cpp` | 2 | 2 | 0 | 0 | 0 | 0 | assembles | - |
| `game/map_cells.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `game/map_field_objects.cpp` | 1 | 0 | 0 | 1 | 0 | 0 | 1 diagnosed asm line | 702 (1) |
| `game/menu_windows.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/mode_tasks.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 1 diagnosed asm line | 388 (1) |
| `game/move_cmds.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/prim.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `game/psx_gte_float.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `game/psx_gte_transform.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `game/rest_1e.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/rest_3e.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/rest_3f.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/rest_4f.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/save_menu.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 2 diagnosed asm lines | 1303 (1), 1310 (1) |
| `game/scena_calls.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 2 diagnosed asm lines | 594 (1), 982 (1) |
| `game/scenario_harness.cpp` | 1 | 0 | 0 | 0 | 1 | 0 | assembles | - |
| `game/shell.cpp` | 2 | 0 | 0 | 2 | 0 | 0 | 6 diagnosed asm lines | 131 (5), 259 (1) |
| `game/sound.cpp` | 5 | 5 | 0 | 0 | 0 | 0 | assembles | - |
| `game/sound_rest.cpp` | 6 | 1 | 0 | 5 | 0 | 0 | front end stops (section 7); blocks judged in isolation | 177 fails (1), 184 fails (1) |
| `game/sprite_draw.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `game/sprite_records.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `game/sprt_draw.cpp` | 5 | 5 | 0 | 0 | 0 | 0 | assembles | - |
| `game/task_sched.cpp` | 6 | 0 | 0 | 6 | 0 | 0 | 32 diagnosed lines | 74 (15), 138 (9), 176 (8) |
| `game/tex_cells.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `game/text_draw.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | assembles | - |
| `game/world_map.cpp` | 1 | 1 | 0 | 0 | 0 | 0 | assembles | - |
| `hook/exe_host.cpp` | 4 | 0 | 4 | 0 | 0 | 0 | assembles | - |
| `hook/gapwatch.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | front end stops (section 7); block judged in isolation | - |
| `hook/snapshot.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| `hook/stackwatch.cpp` | 4 | 0 | 2 | 2 | 0 | 0 | front end stops (section 7); blocks judged in isolation | - |
| `render/render_d3d11.cpp` | 2 | 0 | 2 | 0 | 0 | 0 | assembles | - |
| **70 files** | **174** | **104** | **23** | **30** | **6** | **11** | 58 compile to an object; 8 stop on asm alone; 4 stop in the front end | **15 blocks** |

Every diagnostic on an asm line is one of two: `instruction requires: Not
64-bit mode` (51 diagnostics on 49 lines in the files, `grep -c` over
`scratch/x64log/*.log`, and 2 more in the isolation TU: `pushl` / `popl` /
`push` / `pop` of a 32-bit register, `jmp *%eax`, `jmp *%ecx`) and `invalid
operand for instruction` (2 lines, `mp3.cpp`'s block in the isolation TU:
`pushl` of an operand whose `"r"` register is 64-bit because the C++
operand is a pointer). Nothing else failed to assemble:
`esp` as a 32-bit register, `fs:`, a symbol with a leading underscore, an
absolute 32-bit address, a `call rel32` to an undefined symbol all pass the
assembler and are judged in section 4.

## 3. The x87 and control-word blocks: 127 of 127 assemble, and stay asm

Every x87 block (104, 36 files) and every control-word block (23, 11 files)
assembled under the x86_64 driver as written: 36 x87 files and 10 cw files
compiled to an object with no diagnostic; the one x87 block in a file that
stops in the front end (`sound_rest.cpp:111`, `SndBuf_SetVolume`'s
`fildl ... fistpll` under a copied control word) and the two cw blocks in
another (`stackwatch.cpp:210-211`, `fnstcw` / `stmxcsr`) assembled in the
isolation TU. The objects hold the x87 the blocks wrote: over the 58
objects, `llvm-objdump -d` counts 562 `fstps`, 463 `flds`, 231 `fildl`,
172 `fldcw`, 79 `fnstcw`, 65 `fistpll`, 32 `fldt`, 14 `fstpt` (the 80-bit
spills), 37 `fucompi` - instructions the x86-64 code generator never emits
for `float` / `double` (SSE), so they are the blocks'.

Why they are right as well as assembled: their operands are memory
(`"m"`), `"i"` immediates and the `st` clobber; the few general registers
are 32-bit scratch of a 32-bit value (`sound_rest.cpp:111`'s `%%ax` /
`%%ah` on a control word; `"a"` / `"q"` outputs of `std::uint32_t`), legal
and the same in 64-bit mode. A `"m"` operand that names an image place
through `*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(Img(...)))`
widens to a native pointer in the C++ and the asm never sees the width.

**They stay asm in this round** (the round doc's section 3 rule): the
control word, the 80-bit intermediates and `fistp` under chop are what the
original computes, and a C++ spelling is a divergence until fuzzed. B1 does
not touch them; the round that leaves x86 owns the question.

## 4. The stack, fs and other blocks, one by one

Columns: what the block does; the x86_64 assembler's verdict; whether the
assembled code would be right on x86-64; the treatment of the round doc's
three (**C++** proved by the hosted fuzz; **pair**, the i686 asm kept byte
for byte and an x86_64 asm beside it in a shape a third variant can join,
the owner's call 2; **thunk** without `naked` where the function only
re-arranges arguments for a Capcom register contract). The fuzz column says
whether a hosted shadow fuzz covers the file (`<file>_fuzz.cpp` exists:
`battle_misc`, `field_e2`, `field_misc`, `field_modes`, `magic_s28`,
`map_field_objects`, `save_menu`, `scena_calls`, `shell`, `sound`,
`sound_rest`, `task_sched`; `ls src/game | grep _fuzz`).

### 4.1 The 30 stack blocks

| Block | Does | Assembles | Right on x86-64 | Treatment |
|---|---|---|---|---|
| `task_sched.cpp:74` `Task_RunAll` | pushes ebx ecx esi edi ebp, stores esp, walks the four records, enters a task by `esp = saved; 6 pops; ret` | no (15 lines) | - | **pair**: the x86_64 switch with `rsp`, the Win64 callee-saved set (`rbx rbp rsi rdi r12..r15`, `xmm6..xmm15`), the record's saved-esp slot a 32-bit word that holds a low stack (XA2's arena); the `eax` left untouched as the i686 half leaves it |
| `task_sched.cpp:125` `Task_BackToScheduler` | the landing: esp = the scheduler's, `jmp` to the walk's next record | **yes** | no: `movl ...(%ebx), %esp` loads the low 32 bits of `rsp`; `jmp .Ltask_sched_next` is a label of the other block's asm, so the two are one unit | **pair**, with `Task_RunAll` |
| `task_sched.cpp:138` `Task_SetStackBase` | `Task_StackTop = esp - 0x4000`, eax left that value; the armed path calls the stackwatch with everything pushed | no (9 lines: `pushl`, `popl`, `pushfl`, `pushal`, `popal`, `popfl`) | - | **pair**; the x86_64 half takes the top from XA2's arena, not from `rsp`; `pushal` has no 64-bit form, so the armed path saves the registers by name |
| `task_sched.cpp:176` `Task_Sleep` | the record's state 1 and sleep word; the six pushed under the return address; esp saved; the landing | no (8 lines) | - | **pair** |
| `task_sched.cpp:206` `Task_Restart` | state 2; a fresh esp `Task_StackTop - slot * 0x4000 - 0x1C`; the entry stored above the six dwords; the landing | **yes** | no: `movl 4(%esp), %eax` reads the i686 argument slot; the x86-64 caller put `entry` in `rcx`; the six-dword frame is the i686 pop order | **pair** |
| `task_sched.cpp:231` `Task_Exit` | six record dwords zeroed; the landing | **yes** | no: the `_Bof3_ImageMove` load (4.4) and the shared label; the arithmetic itself is fine | **pair** |
| `mode_tasks.cpp:375` `JumpSave` | a `setjmp` of ebx esi edi ebp esp and the return address, returns 0 | **yes** | no: it is the i686 register set, `leal 4(%esp)` the i686 frame | fuzz-only (section 5): `setjmp` / `longjmp` of the CRT, or `__builtin_setjmp` / `__builtin_longjmp`, on both widths; or the pair moved into `mode_tasks_fuzz.cpp` |
| `mode_tasks.cpp:388` `JumpBack` | the `longjmp`: registers back, esp back, returns 1 at the saved address | no (`jmp *%ecx`) | - | with `JumpSave` |
| `area_slope.cpp:153` `CallFramed` | calls `fn` with esp on a copy of a 160-byte block, copies the stack back | no (5 lines: `push`, `pop`) | - | fuzz-only (section 5); stays i686 under the fuzz's `#if` or moves to a fuzz TU |
| `battle_misc.cpp:162` `Battle_StatusTint` | `testb $0x80, 4(%esp)`: bit 7 set -> `jne` into the body (eax its); else bare `ret`, eax the caller's | **yes** | no: `4(%esp)` is the i686 argument; x86-64 has it in `ecx` | **thunk**: C++ `if (status & 0x80) return Body(); return 0;` - the "eax the caller's" path is read by two callers of the original (`0x44F40E`, `0x44F446`, both in `Battle_InflictStatus`, ours), which B1 reads: if ours do not return it on, the fuzz (`Answer::kGarbage` in `boss_harness.cpp:794`) already does not compare it |
| `field_e2.cpp:126` `Scenario_CallB` | tail jump through chapter table B at `(n & 0xFF)`, the caller's arguments in place | **yes** | no: `movl 4(%esp), %ecx` reads the i686 argument; `_Bof3_ImageMove` absolute (4.4); `jmp *(%edx,%ecx,4)` under a 32-bit address override | **pair**: the x86_64 half `movsbl` the chapter, `andl $0xFF, %ecx` on the register argument, `jmp *(%rdx,%rcx,4)` - a tail jump forwards `rcx rdx r8 r9` and the caller's stack as the i686 one forwards the pushed words; a third variant is the same idea in its registers |
| `field_modes.cpp:140` `Scenario_CallA` | the same through table A | **yes** | no, as above | **pair**, the same shape |
| `field_misc.cpp:333` `MoveCmd_TestFB` | `jmp _FieldMisc_TestFB`: the u8-declared entry hands the body's whole eax back | **yes** (`IMAGE_REL_AMD64_REL32` to `_FieldMisc_TestFB`) | yes once the symbol is spelled without the underscore: a tail jump forwards the x86-64 register arguments unchanged and the body returns in `eax` | **thunk kept naked**: one spelling for both widths through `__USER_LABEL_PREFIX__` (`shell.cpp:34`'s `SHELL_SYM`); `0 differ` on i686 |
| `field_misc.cpp:336` `MoveCmd_TestFC` | the same | **yes** | as above | as above |
| `magic_s28.cpp:836` `Port_DroppedCall` | `ret` and seven `int3` (the five bytes `BOF3X_ORIGINAL`'s jmp overwrites, hosted) | **yes** | yes | **no change** |
| `map_field_objects.cpp:702` `ScriptFlags_Clear40` | `pushl %eax; call body; addl; ret`: the caller's eax to the body, which returns it with the new byte in al | no (`pushl`) | - | **thunk**: C++ `Body(0)` or the body made `void`; the eax passthrough was for two Capcom tail-jumpers (`0x417D49` in `Area111_TailGate`, `0x543A5D` in `Scena03_Scene5`), both ours now, which B1 reads for whether they return it on; the fuzz covers the file |
| `save_menu.cpp:1303` `Music_IsPlaying` | `push %ecx; call body(ecx); add; ret`: the caller's ecx is the uninitialised status local the original tests when `GetStatus` fails | no (`push`) | - | **pair**, the x86_64 half one instruction: `jmp Body` - the caller's `ecx` is already the first argument. Or **thunk** `Body(0)` once B5's SDL3 output removes `GetStatus` (the local is then never read uninitialised) |
| `save_menu.cpp:1310` `LoadMenu_Error` | the same for the load menu's error targets (the high byte of the slot) | no (`push`) | - | as above; `save-menu.md`'s "two uninitialised locals, kept" names what the value is |
| `scena_calls.cpp:594` `ScenaCall_SaveAndLeave` | `pushl %ecx; call body(ecx)`: the 4-byte local the member ids go into is the caller's ecx, which is `Scenario_CallA/B`'s `n & 0xFF` | no (`pushl`) | - | **pair**, x86_64 half `jmp Body` with `ecx` = the index, which the `Scenario_CallA/B` pair leaves in `ecx` on both widths (`scena_calls.md` "the caller's ecx as data") |
| `scena_calls.cpp:982` `Scena15_LeaveAllParty7` | the same | no (`pushl`) | - | as above |
| `shell.cpp:131` `Cfg_Load` | lays out the original's `0x3C`-byte frame with edi, esi under it and calls `Shell_CfgLoadFrame(frame)` | no (5 lines) | - | **C++**: a local `unsigned char frame[...]` shaped as the original's, passed to the body; what `Cfg_SetKeyTable` copies past it is bounded by DIV-0078 already; `shell_fuzz.cpp` is the gate |
| `shell.cpp:259` `Game_Init` | `call Shell_GameInitBody; jmp *%eax`: the body's return is `Task_SetStackBase`, entered by a jmp so the task stacks hang from WinMain's esp | no (`jmp *%eax`) | - | **pair** or **C++** by XA2's task-stack design: with a low arena of our own the jmp is moot and `Body(); Task_SetStackBase();` is right on x86-64; on i686 that moves `Task_StackTop` by one frame, so the i686 half stays asm unless the state hash says the stack base is invisible to it (section 9) |
| `sound_rest.cpp:69` `Sound_StopMusic` | `jmp *(_SoundRest_g+8)`: a tail jump to `Music_Halt` through the callee table | **yes** (isolation; `IMAGE_REL_AMD64_ADDR32`) | no: with 8-byte function pointers `halt` is at `+16` (`sound_rest_callees.h:52`'s `static_assert` says so), and the absolute 32-bit address needs the DLL below 2 GB | **thunk**: C++ `return SoundRest_g.halt();` (the ecx it hands on is the Music_Halt row's); or the pair's `jmp *SoundRest_g+16(%rip)` |
| `sound_rest.cpp:72` `Sound_ResumeAll` | the same to `Music_Resume` (`+12`, `+24` on x86-64) | **yes** | no, as above | as above |
| `sound_rest.cpp:77` `Sound_MusicPlaying` | the same to `Music_IsPlaying` (`+16`, `+32`) | **yes** | no, as above | as above |
| `sound_rest.cpp:177` `Music_Halt` | `push %ecx; call body(ecx)`: the status local, as `Music_IsPlaying` | no (isolation: `push`) | - | as `Music_IsPlaying` (pair with a one-instruction half, or the thunk under B5) |
| `sound_rest.cpp:184` `SndStream_IsPlaying` | the same | no (isolation) | - | as above |
| `mp3.cpp:174` `Decode` | switches esp to a 256 KB block of ours around one `__stdcall` call | no (isolation: `pushl` of a 64-bit register) | - | **pair**: the x86_64 half switches `rsp`, keeps the 32-byte shadow space and 16-byte alignment, calls with `rcx`, `rdx`; or a fiber / thread with its own stack on both widths (a design choice for XA2's stack section, since the present already runs on a fiber) |
| `stackwatch.cpp:242` | `movl %%esp, %0` into a `std::uint32_t` | **yes** | only while the main thread's stack lies below 4 GB, which Windows does not promise a 64-bit process | **C++** `reinterpret_cast<std::uintptr_t>(&local)` or the width's register name under `#if`; a hook file, not a game file |
| `stackwatch.cpp:294` | calls `Task_SetStackBase` with ecx, edx set and read back | **yes** (`call rel32` to `_Task_SetStackBase`) | the check it makes (ecx, edx kept across a Capcom register contract) has no x86-64 meaning; the symbol needs the prefix macro | **pair** with the task switch, or the check made i686-only beside the i686 half |

### 4.2 The 6 `fs:` blocks

All six assemble: `fs:` is a legal segment override in 64-bit mode. All six
are wrong there: Windows x86-64 keeps the TEB at `gs:` (NT_TIB64:
`StackBase` `+8`, `StackLimit` `+0x10`, `Self` `+0x30`; TEB64
`ProcessEnvironmentBlock` `+0x60`; PEB64 `ImageBaseAddress` `+0x10`), and the
32-bit layout at `fs:4`, `fs:8`, `fs:0x30` is the WoW64 TEB's, which a native
64-bit process does not have at `fs`.

| Block | Does | Treatment |
|---|---|---|
| `image_base.h:62` `ImageBase()` | `fs:0x30` the PEB, `+8` its `ImageBaseAddress` | B4: `gs:0x60`, `+0x10`, under `#if` by width in this one header (the round doc's section 2 allows the header); or `NtCurrentTeb()->ProcessEnvironmentBlock->ImageBaseAddress`, which the compiler inlines to the same two loads and which is width-correct by itself - the comment at `:52-58` says why the asm is not `volatile` and touches no memory the compiler knows, which `NtCurrentTeb()` keeps |
| `boss_harness.cpp:426`, `:427` (`Readable`) | `fs:4` StackBase, `fs:8` StackLimit | **C++**: `NtCurrentTeb()`'s `NT_TIB` `StackBase` / `StackLimit`, width-correct in `<winnt.h>`; the surrounding reads of the image's PE header through the 32-bit `image = 0x400000` are B2's |
| `boss_harness.cpp:437`, `:438` (`Writable`) | the same | the same |
| `scenario_harness.cpp:142` `OnStack` | `fs:4` StackBase | the same |

### 4.3 The 11 "other" blocks

All assemble and all are right on x86-64:

| Block | Does | Note |
|---|---|---|
| `event_objs.cpp:90`, `event_ops.cpp:168`, `event_script.cpp:95`, `frame_callees.cpp:42`, `kind2_object.cpp:72`, `menu_windows.cpp:151` | `cltd; idivl`: the original's signed 32-bit division, divide fault included | 32-bit operands, the same instruction; the round doc counted seven, the classifier finds six (the TSV's `mnemonics` column, `grep cltd`) |
| `event_ops.cpp:161` `DirectionInPointer` | `movb 8(%1), %b0`: a byte read into the low byte of a 32-bit copy of a pointer (clang 22.1.8 folds the C++ spelling, `event-ops.md` 8) | the `"r"(sc)` operand is a 64-bit register and `8(%rax)` is right; the 32-bit copy of the pointer (`Address(sc)`) is B2's narrowing |
| `battle_items.cpp:143` `AnimationInPointer` | `movb 8(%1), %b0; addb %b2, %b0`, the same shape | the same |
| `text_draw.cpp:44` `GlyphTrap` | `mov $0x1000, %dx; in %dx, %al`: the original's privileged fault, reproduced | `in` exists in 64-bit mode and faults the same way (`#GP` in user mode) |
| `gapwatch.cpp:576` `PlantedRead32` | one unaligned `movl (%1), %0`, out of line so the fault's address is one instruction | isolation TU; right |
| `rest_1e.cpp:180` | an empty asm with `"+r"(upper)`: a register barrier against a fold | right on any width |

### 4.4 The spellings: 6 aliases, 29 underscored operands, 10 `_Bof3_ImageMove` loads

The i686 ABI prefixes every C symbol with `_`; the x86-64 ABI prefixes
nothing (`__USER_LABEL_PREFIX__` is `_` and empty respectively; `shell.cpp:34`
already spells its two `call` operands through it). The assembler accepts an
unknown symbol, so every underscored operand *assembled* under x86_64 and
would fail at link.

- **The 6 aliases** (`sound.cpp:77-81` `_Sound_PlayEffect`, `_Music_Play`,
  `_Music_FadeOutStop`, `_Music_FadeIn`, `_Music_FadeOut`; `sound_rest.cpp:65`
  `_Sound_PauseAll`): an `int`-returning definition under the assembler name
  of the `void` declaration the generated header makes. x86-64 spelling:
  `__asm__("Music_Play")` - one macro `BOF3_ASM_NAME(x)` built from
  `__USER_LABEL_PREFIX__` in a common header, used by all six. Nothing else
  changes: the alias is a name, not code.
- **The 29 underscored operands** in 10 files (the classifier over the
  comment-stripped templates; the round doc's 39 counted 11 `call _ftol`
  lines that are comments quoting the original, `effect_1a`, `_3b`, `_3c`,
  `_3d`, `_4b`, `_5b`, and missed `cmpb $0, _StackWatch_Armed`):
  `task_sched` 13 (8 `_Bof3_ImageMove`, 3 `jmp _Task_BackToScheduler`,
  `_StackWatch_Armed`, `_StackWatch_AtSetStackBase`), `sound_rest` 5 (3
  `_SoundRest_g`, `_SoundRest_HaltBody`, `_SoundRest_StreamPlayingBody`),
  `field_misc` 2, `save_menu` 2, `scena_calls` 2, `battle_misc` 1,
  `field_e2` 1, `field_modes` 1, `map_field_objects` 1, `stackwatch` 1. The
  spelling for a `call` / `jmp` / `cmpb` of a symbol: the same prefix macro
  in the template string (`"jmp " BOF3_SYM(FieldMisc_TestFB)`). Where the
  block becomes C++ or a pair, the operand goes with it.
- **The 10 `_Bof3_ImageMove` loads** (`task_sched` 8, `field_e2.cpp:126`,
  `field_modes.cpp:140`; the variable at `host.cpp:25`): `movl
  _Bof3_ImageMove, %ebx` assembles to a 32-bit absolute load
  (`IMAGE_REL_AMD64_ADDR32` in `field_e2`'s object, `llvm-objdump -r`), which
  the 64-bit linker refuses unless the DLL lies in the low 2 GB
  (`/LARGEADDRESSAWARE:NO` territory). The x86-64 spelling is RIP-relative:
  `movl Bof3_ImageMove(%rip), %ebx`, and the register then serves as a
  64-bit base (`%c0(%rbx,%rdx)`) with the move zero-extended, right while the
  image lies below 4 GB (the narrow design's premise). All ten sit in blocks
  that are pairs anyway (4.1), so no block keeps the absolute form.

## 5. The 27 naked functions: who calls them in the exe host

The exe host maps no Capcom code: every code word is a bind-table row that
jumps to our function (`bind.cpp`, [`relocatable-image-design.md`](relocatable-image-design.md)
4.1), and `bof3::orig::X()` is a call through that row
(`symbols.gen.h`'s `detail::Original`, `::bof3::Code(va)`). So a caller is
either our C++ (direct, through a `*_CALL` harness macro, or through a
callee table of ours) or a data word of the image bound to ours. Of the
10,100 `[[func]]` rows, 12 have no `impl` (`tomllib` over `symbols.toml`:
`Fmv_EnterFullscreen`, `Crt_ftol`, `Crt_GetPtd`, the eight `Mp3_*`,
`Gfx_PackRgb`); none of them calls a naked function. The four Capcom call
sites the comments name as the reason for a register contract lie in
functions that are ours: `0x44F40E` and `0x44F446` in
`Battle_InflictStatus`, `0x417D49` in `Area111_TailGate`, `0x543A5D` in
`Scena03_Scene5`, `0x4FC079` in `BattleActor_PlaySound`, `0x495BC0` /
`0x495C50` in `GameMode3_Enter` / `GameMode3_Leave` (the `[[func]]` row
containing each address; all `impl`). Callers counted by `grep -rnw` over
`src/` without fuzz files.

| Naked function | File | Reached in the exe host by | Exists for | Reachable |
|---|---|---|---|---|
| `Task_RunAll` | `task_sched.cpp:73` | `win_main.cpp:779` `bof3::orig::Task_RunAll()` once a frame | the esp switch | yes |
| `Task_BackToScheduler` | `:124` | the `jmp` of `Task_Sleep`, `Task_Restart`, `Task_Exit` alone (no C++ caller; its own entry so `BOF3X_ORIGINAL` can switch it, hosted) | the landing inside the walk | yes, from the asm |
| `Task_SetStackBase` | `:137` | `Game_Init`'s `jmp`, `win_main.cpp:685` (restart), `shell_callees.h:84`, `stackwatch.cpp:294` | `Task_StackTop` from the arrival esp; eax left it | yes |
| `Task_Sleep` | `:172` | `SH_CALL` sites (`effect_1e`, `field_e2`, `mode_rest`, `rest_3g` and more), callee tables of `field_event`, `field_modes`, `mode_flow`, `mode_states`, `mode_tasks` | the six pushed in pop order under the return address | yes |
| `Task_Restart` | `:204` | `BH_CALL` / `SH_CALL` in `battle_e1`, `scena_sc15` (4), `save_menu_callees.h:142`'s code VA | the fresh stack and the landing | yes |
| `Task_Exit` | `:229` | `field_modes_callees.h:43`'s code VA, `mode_flow`, `title_states` callee tables | the landing | yes |
| `JumpSave` | `mode_tasks.cpp:374` | `RunTask` inside `mode_tasks::SelfTest` only (`:627`), `BOF3X_SHADOW=mode_tasks` | a `setjmp` the fuzz ends a task with | **no** (the exe host refuses `BOF3X_SHADOW`, `exe_host.cpp:85`) |
| `JumpBack` | `:387` | the fuzz's `Task_Sleep` recorder (`:442`) | its `longjmp` | **no** |
| `CallFramed` | `area_slope.cpp:152` | `area_slope::SelfTest` only (`:235`, `:240`) | the fuzz's framed call | **no** |
| `Battle_StatusTint` | `battle_misc.cpp:161` | 8 `BH_CALL` / `MH_CALL` sites, `enemy_ai_ops.cpp:46`'s table, `battle_setup_callees.h:90`'s code VA | eax the caller's on the no-tint path (two Capcom returners, ours now) | yes |
| `Scenario_CallB` | `field_e2.cpp:122` | 10+ `SH_CALL` sites in `scena_sc*`, `scena_sc3_callees.h:103` | the caller's arguments forwarded unread | yes |
| `Scenario_CallA` | `field_modes.cpp:136` | `SH_CALL` / `BH_CALL` sites in `scena_sc*`, `boss_sc`, callee tables of `field_modes`, `mode_states` | the same | yes |
| `MoveCmd_TestFB` | `field_misc.cpp:332` | 14+ `AH_CALL` sites in `area_w*`, tables in `mode_flow`, `mode_states`, `move_groups`, `move_script` | the body's whole eax behind a u8 signature | yes |
| `MoveCmd_TestFC` | `:335` | `AH_CALL` / `SH_CALL` in `area_w1a`, `effect_5d`, `effect_5f`, tables in `event_leader`, `move_groups`, `move_script` | the same | yes |
| `Port_DroppedCall` | `magic_s28.cpp:835` | 10+ sites and tables (`battle_turn_steps`, `battle_fx_tasks`, `mode_flow`, `move_groups`, `magic_c1` by address) | the bare `ret` padded for the hosted jmp | yes |
| `ScriptFlags_Clear40` | `map_field_objects.cpp:701` | 14+ `AH_CALL` sites in `area_w0b..w1d` and on | eax passed through with al the new byte (two Capcom tail-jumpers, ours now) | yes |
| `Music_IsPlaying` | `save_menu.cpp:1302` | `Sound_MusicPlaying`'s jmp (`SoundRest_g+16`), `sound_in.cpp:40`, `save_menu_callees.h:165` | the caller's ecx as the status local | yes |
| `LoadMenu_Error` | `:1309` | a code word: no C++ call site (`grep -rnw` finds the inject and a comment); `save-menu.md` 72 lists it as step `0xB3` of the load menu's table, "untraced"; reached through the bound table word, from `TitleFlow_Load` (ours; `save-menu.md` "two uninitialised locals, kept") | the same | yes, through a data word |
| `ScenaCall_SaveAndLeave` | `scena_calls.cpp:593` | chapter 9's B[6] and 10's B[3] of the scenario call tables (`scena_calls.md` 146): `Scenario_CallB`'s jmp through the bound word | the caller's ecx (the call index) as the local | yes, through the call tables |
| `Scena15_LeaveAllParty7` | `:981` | chapter 15's A[0] (`scena_calls.md` 179): `Scenario_CallA`'s jmp | the same | yes, through the call tables |
| `Cfg_Load` | `shell.cpp:130` | `win_main.cpp:594` `bof3::orig::Cfg_Load()` | the original's frame under the key-table copy | yes |
| `Game_Init` | `:258` | `win_main.cpp:632` `bof3::orig::Game_Init()` | the `jmp` to `Task_SetStackBase` | yes |
| `Sound_StopMusic` | `sound_rest.cpp:69` | 7+ `AH_CALL` / `SH_CALL` sites, 7 `scena_sc*_callees.h` code VAs | a tail jump: Music_Halt sees the caller's ecx | yes |
| `Sound_ResumeAll` | `:72` | 10+ sites, `quick_menu.cpp:202`, `:478` direct | the same to `Music_Resume` | yes |
| `Sound_MusicPlaying` | `:77` | `mode_rest.cpp:81` `SH_CALL` | the same to `Music_IsPlaying` | yes |
| `Music_Halt` | `:176` | `SoundRest_g.halt` (`Sound_StopMusic`'s jmp, `Sound_PauseAll`'s return), `kOriginals` | the caller's ecx as the status local | yes |
| `SndStream_IsPlaying` | `:183` | `save_menu.cpp:61`'s `Code(kVoiceIsPlaying.v)` through the bind row | the same | yes |

So: **24 reachable, 3 not** (`JumpSave`, `JumpBack`, `CallFramed`, the
fuzz's). Of the 24, the register contract each exists for is one of five
kinds, and in the exe host the other side of every contract is our compiled
C++: (a) the task switch (6, a real contract: the task's saved registers);
(b) arguments forwarded unread (`Scenario_CallA/B`, 2: real on both widths,
the x86-64 form a register-forwarding tail jump); (c) the caller's `ecx`
read as an uninitialised local (6: `Music_IsPlaying`, `LoadMenu_Error`,
`Music_Halt`, `SndStream_IsPlaying`, and the two scena entries where the
`ecx` is `Scenario_CallA/B`'s index); (d) `eax` handed through where a
Capcom caller returned it on (`Battle_StatusTint`, `ScriptFlags_Clear40`,
the three `Sound_*` jumps, `MoveCmd_TestFB/FC`: 7, whose other side is now
our C++ and reads `eax` only as the declared type, unless B1's read of the
named callers finds otherwise); (e) the frame shape (`Cfg_Load`,
`Game_Init`: 2). `Port_DroppedCall` needs nothing.

## 6. The second opinion for XA1: what fails for a reason other than asm

The first diagnostic per file that is not an asm line, from the two runs:

| File | First non-asm diagnostic | What it is |
|---|---|---|
| every `src/game` file, `exe_host.cpp`, `snapshot.cpp`, `gapwatch.cpp` (run 1) | `symbols.gen.h:50493: static assertion failed ... sizeof(GameData) == 0x37b000` (`3654176 == 3649536`, +4,640 bytes), then 2,968 member asserts | **109 `GameData` members are pointer-typed** (`using X_type = void *;` 73, `unsigned char *` 26, `const unsigned char *` 4, `const char *` 2, `unsigned long *` 1, `records::PartyMember *`, `records::BattleTask *`, `records::BattleEnemy *` 1 each: `grep -cE "^\s+using [A-Za-z_0-9]+_type = .*\*;" build/gen/bof3/symbols.gen.h`; 111 `[[data]]` rows with a `*` in `ctype`, two of them outside the struct). Each widens to 8 bytes on x86-64 and shifts every member after it. The round doc's section 0 "no pointer-typed member" read the brace-depth parse, not the `using` lines. **B3**: `gen_symbols.py` emits a 32-bit slot type for a pointer ctype (`std::uint32_t`, the VA) and the accessor macro keeps the `T *` reinterpretation; the typed-data round's `codegen_same.py` gate (`0 differ` on i686). Until then no game file compiles for x86-64 at all, which makes this the first merge of stage B |
| `mp3.cpp:47` | `static_assert ... offsetof(Decoder, frame) == 20` | the decoder record holds pointers; the frame pointer at `+0x14` is Capcom's layout, read by the MP3 glue (DIV-0091). XA2 / B5: the record's pointer fields as 32-bit slots or the assert by width |
| `sound_rest_callees.h:52` | `static_assert ... offsetof(Callees, halt) == 8` | the callee table of function pointers doubles; the three `jmp *(_SoundRest_g+N)` operands name the i686 offsets (4.1) |
| `stackwatch.cpp:162`, `:165`, `:168` | `CONTEXT_EXTENDED_REGISTERS` undeclared; no `Esp`, `Eip`, `Eax`, `ExtendedRegisters` in `_CONTEXT` | the x86 `CONTEXT` record; x86-64's has `Rsp`, `Rip`, `Rax`, `FltSave`. B4 (a hook file), by width |
| `gapwatch.cpp:173`, `:233`, `:285`, `:286`, `:298`, `:312` | no `Esp` / `Eip`; `DWORD *` from `DWORD64 *` (the debug registers `Dr0..Dr3`) | the same record; the hardware watchpoints' registers are `DWORD64` on x86-64. B4 |
| `effect_4b.cpp:84`, `effect_5b.cpp:58`, `map_cells.cpp:80` | `-Wunused-function` (`Shards`, `SetUL`), `-Wbitwise-instead-of-logical` | the only warnings of the run; the i686 driver gives the same three on the same lines (`scratch/i686_warn.py`, the files' own compile commands), so they are not width issues |

No file failed on a narrowing cast, `long`, `__cdecl` / `__stdcall` or
`sizeof(void *)`: Win64 is LLP64 and the calling-convention keywords are
ignored, as the round doc's section 0 said. The 384 narrowings and 950
widenings compile (they are `static_cast` through `std::uintptr_t`) and are
wrong only at run time above 4 GB - B2's `Low32()`.

## 7. The method's limits

- **The patched header is scratch.** It changed 109 `using` lines of a copy
  of `symbols.gen.h` and nothing else; the asm verdicts do not depend on
  it, since no asm block names a `GameData` member by type.
- **Twelve blocks were judged in copies** (`scratch/iso_blocks.cpp`), not in
  their files: `mp3.cpp:174`, the six of `sound_rest.cpp`, `gapwatch.cpp:576`,
  the four of `stackwatch.cpp`. Each copy keeps the operand constraints and
  the C++ types behind them; the i686 driver compiles the copy with no
  diagnostic, the x86_64 driver gives the verdicts of section 4.
- **Assembled is not linked and not run.** Undefined `_`-prefixed symbols,
  absolute 32-bit relocations and `fs:` reads pass the assembler; section 4's
  "right on x86-64" column is a reading, and stage C's link and the state
  hash are the proof.
- **No `x86_64` object was linked** and no game was run, per the brief.

## 8. B1's work list, in merge-able units

Order by dependency: unit 0 first (nothing compiles without it), then the
spellings, then the rest in any order, one PR each, the gate named.

| Unit | Files | Blocks | Treatment | Gate |
|---|---|---|---|---|
| 0 (B3, not B1, but first) | `tools/gen_symbols.py`, the 111 pointer ctypes | - | the 32-bit slot type for a pointer ctype (section 6) | `codegen_same.py` `0 differ` on i686 |
| 1 the spellings | a common header (`BOF3_SYM` / `BOF3_ASM_NAME` from `__USER_LABEL_PREFIX__`); `sound.cpp:77-81`, `sound_rest.cpp:65` (aliases); `field_misc.cpp:333`, `:336`; `shell.cpp:34` folded into it | 6 aliases, 2 blocks, every operand the later units keep | the prefix macro | `codegen_same.py` `0 differ` (names only) |
| 2 the task switch | `task_sched.cpp` (6), `stackwatch.cpp:294` (its check), `mp3.cpp:174` if XA2 keeps the esp switch | 7-8 | **pair**: the i686 text byte for byte beside an x86_64 switch; `Task_BackToScheduler`'s shared label kept inside the pair; the slot and slice from XA2 (the arena below 4 GB, the slice from a stackwatch measurement); the file's layout leaves room for a third variant (one `#if` block per width, the record offsets shared) | `codegen_same.py` on i686; `task_sched_fuzz.cpp`; the state hash in stage C |
| 3 the call tables | `field_e2.cpp:126`, `field_modes.cpp:140` | 2 | **pair**, the x86_64 half a register-forwarding tail jump with RIP-relative `Bof3_ImageMove` | `codegen_same.py`; `field_e2_fuzz.cpp`, `field_modes_fuzz.cpp` |
| 4 the ecx entries | `save_menu.cpp:1303`, `:1310`; `sound_rest.cpp:177`, `:184`; `scena_calls.cpp:594`, `:982` | 6 | **pair**, each x86_64 half one `jmp Body` (ecx is the first argument); the scena two depend on unit 3 leaving `ecx` = the index on both widths | `codegen_same.py`; `save_menu_fuzz`, `sound_rest_fuzz`, `scena_calls_fuzz` (the chosen-ecx rounds) |
| 5 the sound jumps | `sound_rest.cpp:69`, `:72`, `:77` | 3 | **thunk** (C++ through `SoundRest_g`), or the pair with the doubled offset; B5 rewrites the bodies for SDL3, so the thunk is the smaller change | `sound_rest_fuzz.cpp` |
| 6 the eax thunks | `battle_misc.cpp:162`, `map_field_objects.cpp:702` | 2 | **thunk** without `naked`, after reading the named callers (`Battle_InflictStatus`, `Area111_TailGate`, `Scena03_Scene5`) for a read of the whole `eax` | `battle_misc_fuzz`, `map_field_objects_fuzz` |
| 7 the shell | `shell.cpp:131`, `:259` | 2 | `Cfg_Load` **C++** with the frame shaped; `Game_Init` by XA2's stack design (pair or C++) | `shell_fuzz.cpp`; the state hash |
| 8 the fuzz helpers | `mode_tasks.cpp:375`, `:388`; `area_slope.cpp:153` | 3 | `setjmp` / `longjmp` or the blocks moved to the fuzz TUs, which compile for i686 alone | the fuzz itself (`BOF3X_SHADOW=mode_tasks`, `=area_slope`) |
| 9 the TEB reads | `boss_harness.cpp:426-438`, `scenario_harness.cpp:142` | 5 | **C++** `NtCurrentTeb()->StackBase` / `StackLimit` | `codegen_same.py` may differ by the inlined load; the harness self-tests |
| 10 (B4) | `image_base.h:62`, `stackwatch.cpp:242` and its `CONTEXT`, `gapwatch.cpp`'s `CONTEXT` | 2 + the C++ | by width in the hook and common files | `0 differ` on i686 |
| none | the 104 x87, 23 cw, 11 other, `magic_s28.cpp:836` | 139 | unchanged | - |

Units 2, 3 and 4 together are the 15 failing blocks plus the 12 assembling
stack blocks that are wrong; 5 to 10 are the rest of section 4. Every unit
but 0 and 10 is a game file and lands in the i686 build first (the round
doc's section 2).

## 9. Not verified

- Whether the state hash sees `Task_StackTop` and the records' saved esp
  (unit 7's `Game_Init` choice): the hash normalises image places, and these
  words hold stack addresses; a diff of two i686 runs with `Game_Init` as
  C++ decides it.
- Whether any of our C++ callers of the seven kind-(d) functions reads
  `eax` beyond the declared return (unit 6's read).
- The x86-64 frames' depth per task slice, and whether the x87 control word
  must be saved across the switch: XA2's measurement, stage C's stackwatch.
- That the 64-bit linker places the DLL where `IMAGE_REL_AMD64_ADDR32`
  relocations resolve: moot once no block keeps an absolute form (4.4), but
  XA2's low-base probe says where the DLL lies.
