// Four spell overlays of Magic_Rows, round nine group S22 (the PSX's
// MAGIC096..MAGIC099.EMI, rows 102, 19, 37 and 36; read one id down, the
// sibling's labels Blizzard, Jolt, Lightning and Myollnir): 56 functions,
// 0x4C8D40..0x4CC96E, taken with the shared harness (magic_harness.h).
// docs/magic_s22.md.
#pragma once

void MagicS22_Inject();

namespace magic_s22 {

// BOF3X_SHADOW=magic_s22: the start-up fuzz, magic_s22_fuzz.cpp. Clones every
// original before MagicS22_Inject patches it.
void SelfTest();

// The callees of the three functions the shared harness cannot compare
// (magic_s22_fuzz.cpp says why): the two actor-matrix pushes, whose GTE
// callees take the vectors by pointer, and the targets' centre, whose
// Battle_ActorIsOut answers decide a divisor. In the game these are the
// callees themselves; their own fuzz points them at recorders.
struct Hooks {
    void (__cdecl* push_matrix)();
    void (__cdecl* rot_trans)(const short* v, long* t, long* flag);
    short* (__cdecl* rot_matrix)(const short* angles, short* m);
    short* (__cdecl* mul_matrix0)(const short* a, const short* b, short* out);
    void (__cdecl* set_rot)(const unsigned long* m);
    void (__cdecl* set_trans)(const unsigned long* m);
    unsigned char (__cdecl* is_out)(unsigned actor);
};
extern Hooks g_hooks;
extern const Hooks kHooks;

}  // namespace magic_s22
