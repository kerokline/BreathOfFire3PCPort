// A C symbol as the assembler spells it, for the hand-written asm and the
// asm-name aliases (docs/x64-asm-policy.md 4.4): the i686 ABI prefixes every
// C symbol with an underscore, the x86-64 ABI prefixes nothing, and
// __USER_LABEL_PREFIX__ is the compiler's word for which ("_" or empty), so
// one spelling serves both widths and a third would need nothing here.
//
//   BOF3_SYM(name)       the spelling inside an asm template:
//                          asm("jmp " BOF3_SYM(FieldMisc_TestFB));
//   BOF3_ASM_NAME(name)  the asm label of a declaration, the definition under
//                        a name the generated header declares with another
//                        signature:
//                          extern "C" int __cdecl Music_PlayEax(unsigned, int) BOF3_ASM_NAME(Music_Play);
//
// Header-only, free of Windows and of every other header of ours.
#pragma once

#define BOF3_ASM_STR2(x) #x
#define BOF3_ASM_STR(x) BOF3_ASM_STR2(x)
#define BOF3_SYM(name) BOF3_ASM_STR(__USER_LABEL_PREFIX__) #name
#define BOF3_ASM_NAME(name) __asm__(BOF3_SYM(name))
