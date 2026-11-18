// The key numbers the engine stores and compares, without a Windows header.
// Two numberings, named apart:
//   kDik*  DirectInput keyboard scan codes (the SDK's DIK_*, <dinput.h>): what
//          the keyboard device's state array is indexed by, what BOF3.CFG's
//          key table and bof3x.ini's key. items hold (docs/controls.md).
//   kVk*   Windows virtual-key codes (the SDK's VK_*, <winuser.h>): what a
//          WM_KEYDOWN carries in its wParam.
// The values are the Windows SDK's and must never change: they are stored in
// the player's files and compared with what the device reports. Each one is
// proved equal to the SDK's name, on Windows, by
// src/launcher/key_codes_check.cpp. `int`, as the SDK's macros are, so a
// switched use site's arithmetic and comparisons keep their types.
//
// Only the names the tree uses are here; add one with its static_assert.
#pragma once

namespace bof3x::input {

// DirectInput scan codes (DIK_*).
constexpr int kDikEscape = 0x01;
constexpr int kDikQ = 0x10;
constexpr int kDikW = 0x11;
constexpr int kDikReturn = 0x1C;
constexpr int kDikA = 0x1E;
constexpr int kDikS = 0x1F;
constexpr int kDikZ = 0x2C;
constexpr int kDikX = 0x2D;
constexpr int kDikC = 0x2E;
constexpr int kDikV = 0x2F;
constexpr int kDikRShift = 0x36;
constexpr int kDikSpace = 0x39;
constexpr int kDikNumpad7 = 0x47;
constexpr int kDikNumpad8 = 0x48;
constexpr int kDikNumpad9 = 0x49;
constexpr int kDikNumpad4 = 0x4B;
constexpr int kDikNumpad6 = 0x4D;
constexpr int kDikNumpad1 = 0x4F;
constexpr int kDikNumpad2 = 0x50;
constexpr int kDikNumpad3 = 0x51;
constexpr int kDikF12 = 0x58;
constexpr int kDikUp = 0xC8;
constexpr int kDikLeft = 0xCB;
constexpr int kDikRight = 0xCD;
constexpr int kDikDown = 0xD0;

// Virtual-key codes (VK_*).
constexpr int kVkReturn = 0x0D;
constexpr int kVkMenu = 0x12;   // either Alt
constexpr int kVkEscape = 0x1B;
constexpr int kVkSpace = 0x20;
constexpr int kVkF1 = 0x70;
constexpr int kVkF2 = 0x71;
constexpr int kVkF3 = 0x72;
constexpr int kVkF4 = 0x73;
constexpr int kVkF5 = 0x74;
constexpr int kVkF6 = 0x75;
constexpr int kVkF7 = 0x76;
constexpr int kVkF8 = 0x77;
constexpr int kVkF9 = 0x78;
constexpr int kVkF10 = 0x79;
constexpr int kVkF11 = 0x7A;
constexpr int kVkF12 = 0x7B;

}  // namespace bof3x::input
