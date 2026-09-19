#include "hook/inject_all.h"

#include "game/file_io.h"
#include "game/save_io.h"
#include "hook/detour.h"

namespace bof3 {

void InjectAll() {
    FileIo_Inject();
    SaveIo_Inject();
    InjectReport();
}

}  // namespace bof3
