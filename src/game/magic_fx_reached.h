// The magic effects the recorded fight casts, reached only through pointers:
// the disc-and-fan effect and its rings (0x4C4FC0..0x4C5110, 0x4AD130..
// 0x4AD2B0), the steal and the thief's double (0x4B54B0..0x4B58C0,
// 0x4F52D0), the Healing Herb's task and sparkle update (0x4B8D70..
// 0x4B9000) and the backdrop dim (0x4FAFF0..0x4FB070). The PSX BMAGIC
// overlays' code, compiled into the exe. docs/magic_fx_reached.md.
#pragma once

void MagicFxReached_Inject();
