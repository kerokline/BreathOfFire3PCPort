// The snapshot core: the quicksave round's group SN (docs/quicksave.md,
// docs/quicksave-round.md section 2).
//
// A quicksave is the game's memory at one point between two logic frames:
//
//   - the span: Capcom's .rdata, .data and .bss and DIV-0062's draw pool -
//     0x5C4000..0x987000 in the exe host (exe_layout.h: .game then .pool),
//     BOF3.exe's 0x5C4000..0x93E000 plus the DLL's pool, by its own extent,
//     in the hosted build;
//   - the task arena: the four task stacks, which live on the MAIN THREAD's
//     stack, 0x10000 bytes below Task_StackTop (task_sched.cpp; not inside
//     the image, as the round's staging assumed) - so a quicksave belongs to
//     the process whose stack it names, and a restore checks that;
//   - one record per participant: a module holding state outside both
//     (Snapshot_Register).
//
// The file BISLPS1X.DAT starts with an ordinary save file (Save_BuildBlock's
// 0x12B0 bytes), so the save menu's summary reader reads it as a save; the
// snapshot half follows, refused loudly by a build that is not the one that
// wrote it.
//
// Nothing here is a replacement of Capcom's code: F6 / F7 are ours
// (docs/quicksave.md, ledger draft A).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bof3 {

// A participant's record, written at a take. Plain bytes; the participant
// owns their meaning and its own versioning inside them.
class SnapshotWriter {
public:
    explicit SnapshotWriter(std::vector<unsigned char>& out) : out_(out) {}
    void Put(const void* data, std::size_t n);
    template <typename T>
    void Put(const T& v) {
        Put(&v, sizeof v);
    }

private:
    std::vector<unsigned char>& out_;
};

// The same record at a restore. A read past its end is a Fatal (the
// participant's bug, never a short read); a restore that leaves bytes unread
// is a Fatal too - rule 4, no silent partial restore.
class SnapshotReader {
public:
    SnapshotReader(const char* tag, const unsigned char* data, std::size_t n) : tag_(tag), p_(data), left_(n) {}
    void Get(void* to, std::size_t n);
    template <typename T>
    T Get() {
        T v;
        Get(&v, sizeof v);
        return v;
    }
    std::size_t Left() const { return left_; }

private:
    const char* tag_;
    const unsigned char* p_;
    std::size_t left_;
};

using SnapshotSaveFn = void (*)(SnapshotWriter&);
using SnapshotRestoreFn = void (*)(SnapshotReader&);
// Optional (SP-A, the audio participant): runs at a restore once the file has
// been checked and before the span is written, so the words it reads are the
// live process's - the participant lets go of what they name (DirectSound
// buffers, heap blocks), which after the span is written it could no longer
// find: the words are then the take's.
using SnapshotDiscardFn = void (*)();

// A module owning state outside the span registers once, at start-up (its
// _Inject): `tag` 1..15 printable characters, unique. Save runs at the take;
// Restore runs after the span, the task arena and the process-owned words
// have been written back, so the span already holds the snapshot's values
// (a participant that needs the live values it had - a device pointer to
// release - keeps its own copy outside the span). A registration after the
// first take or restore, or a second one of a tag, is a Fatal.
void Snapshot_Register(const char* tag, SnapshotSaveFn save, SnapshotRestoreFn restore,
                       SnapshotDiscardFn discard = nullptr);

// The keys' requests (Game_WndProc) and the quick menu's (stage B): nothing
// happens until Snapshot_Pending. Slots are 0x10..0x1F; F7 loads 0x10, the
// newest.
void Snapshot_RequestTake();
void Snapshot_RequestLoad(unsigned slot);

// The line the last take or load showed through Overlay_Text ("Quickload",
// "Quickload refused: ..."), "" before any: the quick menu (QM) reads it
// after Snapshot_Pending ran its load, to stay open on a refusal and say why
// (with our back buffer the overlay line reaches only the log).
const char* Snapshot_LastLine();

// WinMain's loop, after Task_RunAll, Gfx_LinkOTags and the frame count and
// before the next frame's draw - the one point where every task is suspended
// and the main thread holds nothing but the loop's locals. Runs a requested
// take or restore; nothing otherwise. Never from Game_WndProc (a key arrives
// mid-frame).
void Snapshot_Pending();

// InjectAll, last (after DrawPool_Grow, which places the hosted pool): the
// self-test under BOF3X_SHADOW=snapshot (or '*').
void Snapshot_Inject();

}  // namespace bof3
