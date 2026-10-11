// The main thread's stack and the start-up floating-point words, measured for
// state 3 (docs/crt-startup.md sections 4, 8 and 10): what BOF3.exe's game
// thread is given, and - with BOF3X_STACKWATCH=1 - how deep a run goes in it.
//
// The task arena lies inside the main thread's stack: Task_SetStackBase puts
// Task_StackTop 0x66C858 at its arrival esp - 0x4000 and the four tasks take
// 0x4000 bytes each below that (docs/task_sched.md). State 3 links its own
// executable and must give the game thread at least this much.
//
// Instrumentation, not a replacement: it reads memory and logs. It needs no
// DIVERGENCE.md entry.
#pragma once

extern "C" {
// Non-zero once StackWatch_Start has armed BOF3X_STACKWATCH. Read by
// Task_SetStackBase's asm (task_sched.cpp) as _StackWatch_Armed.
extern volatile unsigned char StackWatch_Armed;

// Called from Task_SetStackBase's asm, after it stored Task_StackTop, with
// every register and the flags saved: `arrival_esp` is esp as the call
// arrived (the return address on top). cdecl.
void __cdecl StackWatch_AtSetStackBase(unsigned long arrival_esp);
}

namespace bof3 {

// At start-up, after InjectAll, on the injected thread (the game's main
// thread is still suspended): one line with the image's SizeOfStackReserve /
// SizeOfStackCommit and the main thread's StackBase, StackLimit and
// DeallocationStack from its TEB; one with the x87 control word and MXCSR as
// this (injected) thread holds them. Always logged. With BOF3X_STACKWATCH=1
// also arms the Task_SetStackBase line and the exit line.
void StackWatch_Start();

// At DLL_PROCESS_DETACH: with BOF3X_STACKWATCH=1, the main thread's
// committed stack (VirtualQuery) and TEB StackLimit again, the deepest
// non-zero dword in it, and whether each task slice lies in the committed
// range. Nothing when unarmed.
void StackWatch_Exit();

}  // namespace bof3
