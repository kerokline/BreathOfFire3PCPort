#include "game/gfx_frame.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// original 0x4FD230. Called by WinMain once per logic frame, just before the
// tasks run: aims the packet pool, the ordering-table pointers and the frame
// nodes at this frame's half of the double buffer, and rewinds the unpack
// scratch.
extern "C" void __cdecl Gfx_BeginFrame(void) {
    // DIV-0004. The upload queue is drained only in WinMain's rendered-frame
    // branch, and nothing bounds it. On a rendered pass it is empty by now. On
    // an unrendered one - the catch-up after the window was unfocused or its
    // title bar held - the original leaves it to grow, and the flush that
    // eventually comes unpacks past the end of the scratch buffer into the
    // draw structures. So an unrendered frame drains it here instead: same
    // uploads, same order, before the next frame's logic as always, and
    // before the rewind below so the unpackers see the pointer they expect.
    if (Gfx_UploadQueueCount != 0) {
        BOF3_LOG_FIRST_CALL("DIV-0004: %u uploads left by an unrendered frame, draining",
                            Gfx_UploadQueueCount);
        Gfx_FlushUploadQueue();
    }

    const std::uint32_t b = Gfx_BufferIndex;

    Gfx_PacketNext = Gfx_PacketPools + (b << 16);

    unsigned long* head = Gfx_OtHeads + ((b << 5) >> 2);
    for (unsigned k = 0; k < Gfx_OtPointers_count; ++k) Gfx_OtPointers[k] = head + k;

    // The walk starts 8 bytes into the block per buffer, not one record: the
    // two buffers' records interleave. As the original has it.
    auto* node = reinterpret_cast<std::uint32_t*>(
        reinterpret_cast<std::uint8_t*>(&Gfx_FrameNodes) + b * 8);
    for (int n = 0x38; n != 0; --n) {
        node[0] = 0;
        node[1] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(node));
        node = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(node) + 0x30);
    }

    Gfx_UnpackNext = Gfx_UnpackScratch;
}

void GfxFrame_Inject() { BOF3_INJECT(Gfx_BeginFrame); }
