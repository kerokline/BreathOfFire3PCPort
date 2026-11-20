// launcher/ui/capture_key.h: the bind capture's key numbers.
#include "launcher/ui/capture_key.h"

#include "input/key_sdl.h"

namespace bof3x::ui {

namespace {

// DirectInput's numbers of the keys the capture treats apart (key_codes.h's
// numbering: the set-1 scan code, E0 as bit 7).
constexpr int kEscape = 0x01, kLeftAlt = 0x38, kF10 = 0x44, kRightAlt = 0xB8;

}  // namespace

CaptureTake CaptureTakeKey(int scancode, bool function_keys, int* number) {
    const int dik = input::DikOfScancode(scancode);
    if (dik == 0) return CaptureTake::kNothing;    // the Win32 hook passes a key with no scan code on
    if (dik == kEscape) return CaptureTake::kCancel;
    if (function_keys && (dik == kF10 || dik == kLeftAlt || dik == kRightAlt)) return CaptureTake::kRefused;
    *number = dik;   // DirectInput's number, Pause and NumLock too (D427, fixed 2026-11-20)
    return CaptureTake::kKey;
}

}  // namespace bof3x::ui
