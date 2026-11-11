// bof3::os message box on Windows: MessageBoxA. See os/dialog.h.

#include "os/dialog.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

void ShowMessage(const char* title, const char* text, bool error, bool modal) {
    MessageBoxA(nullptr, text, title, MB_OK | (error ? MB_ICONERROR : 0) | (modal ? MB_TASKMODAL : 0));
}

}  // namespace bof3::os
