// The exe host's start-up, step by step, as Windows runs it today: the order a
// Linux `main` must keep (docs/linux-round.md, "R3: what a Linux start-up
// needs from the PE host", and "Stages LC and LD, staged as one round", group
// C2). This header holds no code; it is the written order wave B codes from.
// Line numbers are src/hook/exe_host.cpp's (dllmain.cpp's for step 2) at the
// commit that wrote this list; a later edit of either file moves them, so read
// each step by its name first.
//
// "Windows" below names what the step takes from the system today; "portable"
// means the step's code already compiles for x86_64-linux-gnu
// (tools/linux_probe/linux_tree.py) and reaches the system only through
// src/os, or not at all.
//
//  1. The executable's entry (tools/make_exe.py, the .text at 0x987000): the
//     Windows loader maps bof3x.exe at its header's base (0x400000, or a
//     developer's make_exe.py --image-base) and loads bof3x.dll, whose C
//     runtime start-up runs the namespace-scope initialisers - among them the
//     tables built from places, which call ImageBase() (image_base.h) before
//     any line of ours below. The entry then takes `show` from
//     GetStartupInfoA (wShowWindow, else SW_SHOWDEFAULT), `hinstance` from
//     GetModuleHandleW(nullptr) and the command line from GetCommandLineA, and
//     hands Engine_Main's answer to ExitProcess.
//     Windows: the PE loader, the import of bof3x.dll!Engine_Main, four
//     KERNEL32 calls. Linux: the start-up maps the image itself at the fixed
//     low address, and must call SetImageBase (image_base.h) before the first
//     initialiser that places an address runs (an initialiser of a higher
//     priority, or the ELF's .preinit_array), else ImageBase()'s Fatal.
//  2. DllMain (dllmain.cpp:42..46): DisableThreadLibraryCalls; LogOpen(the
//     DLL's module); the "attached to process" line; DetectHost
//     (host_win32.cpp), which reads the main module's PE header for the
//     identity and the base and hands them to SetHost (host.cpp: ImageBase()
//     against the base, the 16 MB window at a 64 KB boundary, the linked
//     span, BOF3X_IMAGEBASE, Bof3_ImageMove, the "host:" line); then returns,
//     the exe host's work being Engine_Main's.
//     Windows: the loader's DLL entry, the module handle, the PE header in
//     memory. Linux: LogOpen with the ELF's own module, and SetHost with
//     HostFound{HostKind::Exe, the mapped base, exe_layout::kTimestamp, the
//     image's span} - SetHost itself is portable.
//  3. Engine_Main entered once (exe_host.cpp:584..586) and in the exe host
//     (:587). Portable.
//  4. HandOffToLauncher (:588; the function :555..577): when neither
//     BOF3X_LAUNCHER nor BOF3X_HANDOFF is set and bof3x-launcher.exe lies
//     beside the executable, start it with --no-config and end this process.
//     Windows: GetModuleFileNameW, GetFileAttributesW,
//     SetEnvironmentVariableW, CreateProcessW, TerminateProcess. Linux: the
//     launcher's file name and a process start (the seam's process call,
//     group C7), or the step left out and said so.
//  5. The DLL's own module handle (:589..591), for Crash_Start,
//     StateHash_BeforeInjects, CallTrace_Start and GapWatch_Start.
//     Windows: GetModuleHandleExW from Engine_Main's address. Linux: the
//     ELF's own module (one image: no DLL).
//  6. Two log lines (:592..595): the thread number and the code page.
//     Windows: GetCurrentThreadId, GetACP (CP_UTF8, the manifest's
//     activeCodePage). Linux: os::ThreadId; the code page line has no
//     counterpart (the narrow encoding is UTF-8, os/file.h).
//  7. RefuseHostedSwitches (:597; :85..106): BOF3X_ORIGINAL, BOF3X_SHADOW,
//     BOF3X_MP3=original, BOF3X_MP3_COMPARE, BOF3X_CALLTRACE are Fatals.
//     Windows: GetEnvironmentVariableA (os::GetEnv serves).
//  8. Fence (:598; :181..234, with LinkedEnd :142..149 and LogLowFreeRun
//     :166..179): the largest free run below 16 MB logged; at a moved base
//     the free parts of the linked span 0x400000..SizeOfImage reserved
//     no-access, a module found there a Fatal.
//     Windows: VirtualQuery, VirtualAlloc(MEM_RESERVE, PAGE_NOACCESS),
//     GetModuleFileNameA, the PE header in memory for SizeOfImage. Linux: the
//     start-up's own reservations (os/memory.h; group C3's order), the span
//     from its own layout.
//  9. CheckLayout (:599; :110..136): .stub, .game and .pool are committed,
//     image-backed, writable, and executable as exe_layout.h says.
//     Windows: VirtualQuery (MEM_IMAGE, AllocationBase). Linux: the
//     start-up's own mapping (MEM_IMAGE has no counterpart).
// 10. TrapStub (:600; :497..509): .stub made no-access and read back.
//     Windows: VirtualProtect, VirtualQuery. Linux: os/memory.h.
// 11. CopyData (CopyData, with OpenExe; docs/exe-retire-round.md X2 set this
//     order). A player's start and the developer's differ only in 11a:
//     11a. OpenExe. A player's start (no BOF3X_EXEFILE=1): BOF3X_EXEIMAGE
//          required, nothing opened or tested - ExeFile_Present() false, and
//          every check against the file (11c's DIV-0212, 11d, 11e's
//          CompareWithFile, rdata::Verify, ExeOwn_CheckAtStart) says it did
//          not compare. The developer's
//          half, BOF3X_EXEFILE=1 alone: BOF3.exe in the current directory
//          hashed (SHA-256) and opened through exe_file.h, its section table
//          checked against exe_layout.h - or, without the file,
//          BOF3X_EXEIMAGE required.
//     11b. .game asserted zero; rdata::Map.
//     11c. .data from ExeImage_Source: the cache's image with its table
//          files laid in (ComposeBase), then at a player's start that image
//          hashed against the manifest's [exe] sha256 (call 2, a Fatal on a
//          difference), under the switch a pc-zh image against the file's
//          .data (DIV-0212); or, the developer's half only, from the file
//          (exe_file::ReadAt) when BOF3X_EXEIMAGE is unset.
//     11d. Shell_MapDefaultKeys (against the file's table when opened).
//     11e. ExeImage_LayOptTables, ExeImage_LogMapped, ExeImage_CompareWithFile
//          (a disc-built image against the opened file only).
//     11f. .bss asserted zero.
//     Windows: GetCurrentDirectoryW and GetFileAttributesW (the developer's
//     half only), GetEnvironmentVariableA, the wide path for Sha256Hex. The
//     reader (exe_file.cpp), the image's hash and every fill are portable;
//     a Linux start needs only the player's half.
// 12. Rebase (:602; :325..370): every pointer word of rebase.gen.h checked
//     in its linked range, the data words moved, the code words bound
//     (Bind_Word). Portable.
// 13. BindSlots (:603; x86-64 :423..449, i686 :451..469): the fourteen import
//     slots of kSlots filled with the system's functions (on x86-64 through
//     Bind_ImportThunk's 14-byte thunks) and read back; GetTickCount called
//     through its slot.
//     Windows: GetModuleHandleA, LoadLibraryA, GetProcAddress on DSOUND,
//     GDI32, KERNEL32 and USER32; GetTickCount. Linux: a function of ours
//     for each slot (no system DLL holds one).
// 14. SoundSdl_Start (:604): the DirectSoundCreate slot 0x5C4014 to our
//     Create (DIV-0201). Portable (SDL3).
// 15. SetX87 (:605; :474..484): the x87 control word 0x027F, read back;
//     MXCSR logged. Portable (x86 asm).
// 16. Crash_Start(module) (:607). Portable entry; the catcher per system
//     (crash_win32.cpp, crash_posix.cpp).
// 17. The unattended run's marker (:608..613), with BOF3X_INPUT or
//     BOF3X_SELFTEST_ONLY: a named event Local\bof3x_unattended_<pid> for
//     tools/recipe_saves.py. Windows: CreateEventW, GetCurrentProcessId.
//     Linux: os/named_event.h, or the step said to be Windows-only.
// 18. StateHash_SetImageBase(ImageBase()) (:614), StateHash_BeforeInjects
//     (:615), InjectAll (:616), StateHash_AfterInjects (:617). Portable.
// 19. StackWatch_Start (:618). Portable entry; stackwatch_posix.cpp refuses
//     the switch today.
// 20. BOF3X_SELFTEST_ONLY (:619..624): ExeIcon_SelfTest, the line "self-test
//     only: done, ending the process", LogClose, the end.
//     Windows: exe_icon.cpp (WIC) and TerminateProcess. Linux: the icon's
//     self-test without WIC (group C5), and the process's end.
// 21. The BOF3X_ICON_VRAM line (:625). Portable.
// 22. The watchers and the harness (:626..632): CallTrace_Start,
//     WavDump_Start, InputScript_Start, GapWatch_Start, draw_order::Start,
//     write_watch::Start, stp_census::Start, in that order.
//     Windows: calltrace.cpp, gapwatch.cpp and write_watch.cpp (debug
//     registers, guard pages); on Linux windows_only.cpp defines their
//     entries, each a Fatal when its switch is set. The rest portable.
// 23. SetUnhandledExceptionFilter(Unhandled) (:633; :517..528): an exception
//     nobody handled ends the process as Fatal does, code 3.
//     Windows: the filter. Linux: crash_posix.cpp's handler's end.
// 24. PastProgramName(command_line) (:634; :533..544): the command line past
//     the program name, as Capcom's _wincmdln gave it. Portable; Linux builds
//     the line from argv first.
// 25. Game_WinMain(hinstance, nullptr, line, show) (:635..636), then
//     GapWatch_Flush and the exit line (:637..639); the answer is the exit
//     code. Windows: hinstance and show from step 1. Linux: what the game's
//     window code takes in their place (SDL's window, group C5).
#pragma once
