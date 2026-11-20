// launcher/ui/capture_key.h: the bind capture's key numbers.
#include "launcher/ui/capture_key.h"

#include "input/key_sdl.h"

namespace bof3x::ui {

namespace {

// DirectInput's numbers of the keys the capture treats apart (key_codes.h's
// numbering: the set-1 scan code, E0 as bit 7).
constexpr int kEscape = 0x01, kLeftAlt = 0x38, kF10 = 0x44, kNumLock = 0x45, kRightAlt = 0xB8, kPause = 0xC5;

}  // namespace

int CaptureNumberOfDik(int dik) {
    if (dik == kPause) return kNumLock;   // the Win32 capture: Pause comes as 0x45, not extended
    if (dik == kNumLock) return kPause;   // and NumLock as 0x45 extended
    return dik;
}

CaptureTake CaptureTakeKey(int scancode, bool function_keys, int* number) {
    const int dik = input::DikOfScancode(scancode);
    if (dik == 0) return CaptureTake::kNothing;    // the Win32 hook passes a key with no scan code on
    if (dik == kEscape) return CaptureTake::kCancel;
    if (function_keys && (dik == kF10 || dik == kLeftAlt || dik == kRightAlt)) return CaptureTake::kRefused;
    *number = CaptureNumberOfDik(dik);
    return CaptureTake::kKey;
}

}  // namespace bof3x::ui
