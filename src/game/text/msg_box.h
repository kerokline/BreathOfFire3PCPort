// tools/move_funcs.py (docs/source-layout-round.md). The functions of msg_box.cpp are declared
// by symbols.gen.h (each has an `impl` in symbols.toml).
#pragma once

void TextMsgBoxFromItemUse_Inject();
void TextMsgBoxFromMenuWindows_Inject();
void TextMsgBoxFromModeStates_Inject();
void TextMsgBoxFromMsgPool_Inject();
void TextMsgBoxFromMsgbox_Inject();
