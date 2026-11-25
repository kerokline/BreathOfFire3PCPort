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
// snapshot half follows. Since format 2 (the state-by-name round,
// docs/state-by-name-format.md) a load has three answers: exact in the build
// and process that wrote it; portable in another build or process when every
// task stood at a named place; otherwise refused with its reason, the save
// half offered when it is whole.
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
    // Past n bytes without copying them (a check walking a record); Fatal past the end, as Get.
    void Skip(std::size_t n);

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
// Optional: runs on the participant's record while the file is checked,
// before anything is written. A record this process cannot take back for a
// reason the build check does not see (the ST record's message pool, which a
// language overlay moves and the original does not) is refused here, with a
// short reason for the overlay (`why`) and the log's (`detail`), instead of
// failing in the participant's Restore after the span has been written.
// Returns false to refuse.
using SnapshotCheckFn = bool (*)(SnapshotReader&, std::string& why, std::string& detail);

// A module owning state outside the span registers once, at start-up (its
// _Inject): `tag` 1..15 printable characters, unique. Save runs at the take;
// Restore runs after the span, the task arena and the process-owned words
// have been written back, so the span already holds the snapshot's values
// (a participant that needs the live values it had - a device pointer to
// release - keeps its own copy outside the span). A registration after the
// first take or restore, or a second one of a tag, is a Fatal.
void Snapshot_Register(const char* tag, SnapshotSaveFn save, SnapshotRestoreFn restore,
                       SnapshotDiscardFn discard = nullptr, SnapshotCheckFn check = nullptr);

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

// The last load's answer (docs/state-by-name-round.md 3.1;
// docs/state-by-name-format.md 5), for the quick menu's question (group A4):
// read after Snapshot_Pending ran a load, as Snapshot_LastLine.
//   Exact     the build and process layout that wrote it: restored to the frame
//   Portable  another build or process: the span and the records restored,
//             each task started again at its place
//   RefusedSaveOffered  refused before a byte was written, for `why`; the
//             file's save half is whole (it sums as a save's block), so the
//             menu may offer to load it as a save (Snapshot_ReadSaveHalf)
//   Refused   refused, and there is no whole save half to offer
// None before any load.
enum class SnapshotAnswer { None, Exact, Portable, Refused, RefusedSaveOffered };
struct SnapshotLoadResult {
    SnapshotAnswer answer = SnapshotAnswer::None;
    unsigned slot = 0;
    std::string why;      // short, as the overlay shows it ("another build (not portable)")
    std::string detail;   // the log's
};
const SnapshotLoadResult& Snapshot_LastLoad();

// The keep list (snapshot.cpp kKept): the ranges of the span that are the
// process's, read live before a restore writes the span and put back after
// it. VAs. For the region table's start-up check (group A3).
struct SnapshotKept {
    std::uint32_t at;
    std::uint32_t bytes;
    const char* name;
};
const SnapshotKept* Snapshot_Kept(std::size_t& count);

// The save half of quicksave slot `slot` (0x10..0x1F): its first 0x12B0
// bytes, a save file as BISLPS0?.DAT is, when the file is there and the block
// sums as LoadMenu_Read checks a save's. False with `why` otherwise. For
// tier 0 (group A4): the bytes the title's load reads in place of a slot.
bool Snapshot_ReadSaveHalf(unsigned slot, std::vector<unsigned char>& out, std::string& why);

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
