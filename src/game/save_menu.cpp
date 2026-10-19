// Save, load, the inn and the sound stream - group X of the sixth takeover
// round (docs/takeover-queue-round6.md). Each function read to its last
// instruction with capstone against bof3/BOF3.exe, 2026-09-23
// (docs/save-menu.md), entry and size:
//
//   Snd_LoadBankFile      0x454770 0x9A    Shop_PriceRate      0x583020 0xA3
//   Save_ReadFile         0x454820 0x45    Shop_ScalePrice     0x5830D0 0x23
//   Save_ListFiles        0x4548B0 0xA1    Shop_SellPrice      0x583100 0x40
//   Gfx_ClutStripCopyRow  0x4549F0 0x2E    Shop_InitWindows    0x583140 0xCB
//   Party_RestoreAll      0x580630 0xB1    Shop_Equip          0x583210 0x124 (+ its 5-entry table)
//   SaveMenu_DrawChoices  0x5808E0 0x88    Sound_LoadStream    0x587910 0xE3
//   Menu_DrawBlackScreen  0x580970 0x43    Sound_StreamDone    0x587A00 0x1B
//   Music_IsPlaying       0x5A7020 0x2A
//
// and the title and load flow - task 0's mode 1, the machine 0x587DB0 that
// the catalogue had folded into Snd_LoadBank's extent (group S found that
// function 0xDE bytes long): the step dispatcher, 0x587DC0, the title menu's
// seven states, the new game, the load menu's eight states, the entry into a
// game, and the draws and rolls under them (0x588880..0x588E6F). Twenty-one of
// those were in no function list: they are reached only through the jump
// tables 0x6671F4, 0x66720C and 0x667228.
//
// Faithful but for the quicksave round's draft B (the save list as
// BISLPS0?.DAT, bounded at sixteen; SaveLists_Arm, BOF3X_ORIGINAL=SaveLists).
// Every call out goes through save_menu::g, so the start-up fuzz can stand
// recorders in for the callees.
#include "hook/run_speed.h"
#include "game/save_menu.h"

#include <io.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/save_menu_callees.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "input/sigma.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace save_menu {

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    File_Open, File_Size, File_Seek, File_Read, File_Close,
    Crt_malloc, Crt_free, Crt_sprintf,
    Crt_findfirst, Crt_findnext,
    Rand,
    Snd_LoadBank, Sound_PlayEffect, Music_Stop, Music_Start, Music_SetVolume, Music_IsPlaying,
    Fn<void (__cdecl*)(const void*)>(kVoicePlay), Fn<int (__cdecl*)()>(kVoiceIsPlaying),
    Char_ClearStatus,
    Fn<void (__cdecl*)(unsigned char*)>(kRecalcStats),
    Flags_Test, Party_Count,
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kItemPrice),
    Fn<unsigned char (__cdecl*)(unsigned)>(kShopFlag),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(kInventoryRemove),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(kInventoryAdd),
    Fn<void (__cdecl*)(int, int, int, int, int, unsigned)>(kWindowFrame),
    Fn<void (__cdecl*)(int, int, int, int)>(kWindowBox),
    Fn<void (__cdecl*)(unsigned)>(kWindowBack),
    Fn<void (__cdecl*)(int, int, int, int, unsigned)>(kWindowFrame5),
    Fn<void (__cdecl*)(unsigned, int, int, const unsigned char*)>(kSlotDraw),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, int)>(kSlotCursor),
    Fn<unsigned char (__cdecl*)()>(kMenuYesNo),
    Msg_SystemPtr, Text_DrawAt,
    Fn<void (__cdecl*)(int, int, int)>(kDrawHand),
    Input_AutoRepeat,
    Gpu_SetTile, Gpu_SetSprt, Gpu_SetSemiTrans, Gpu_SetDrawMode, Gpu_GetTPage, Gpu_GetClut, Gfx_CommitPrim,
    Transition_Start, Field_MemberSprite, Sprite_SetAnimation, NewGame_InitCharacters, Scenario_Start,
    Scenario_Load, Field_PartyLoad, PartySet_Select, File_LoadDone, Task_Sleep,
    Fn<void (__cdecl*)(U)>(kTaskRestart),
    Save_ListFiles, Save_ReadFile, Save_ReadSummaries,
    TitleMenu_DrawFrame, TitleMenu_DrawRows, TitleMenu_DrawRow, TitleMenu_DrawPiece, SaveMenu_DrawSlots,
    NewGame_RollGrowth, NewGame_RollSum,
};
Callees g = kOriginals;

}  // namespace save_menu

using namespace save_menu;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Byte(U address) { return At(address)[0]; }
void PutByte(U address, U v) { At(address)[0] = static_cast<unsigned char>(v); }
U Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutFloat(U address, float f) { std::memcpy(At(address), &f, sizeof f); }

// The record views (game/record_views.h) used here: a save
// summary (0x905BC0, 16 of 0x1C), a Save_Directory entry (0x929F40, 16 of
// 0x18), Sprite_Current read afresh (below +0x80, the sprite head every
// family shares) and Field_State, the leader's party record.

unsigned char* Packet() { return Gfx_PacketNext; }

// The state byte both menus run on, stepped as the originals step it.
U State() { return SaveMenu_FlowState; }
void SetState(U v) { SaveMenu_FlowState = v; }

// `rep movsd` then `rep movsb`, forward, each byte read just before it is
// stored.
void CopyForward(U dst, U src, U n) {
    for (U i = 0; i < n; ++i) At(dst)[i] = At(src)[i];
}

// The window pair every load-menu state draws first: 0x575690 with the style
// byte, 0x574AB0(0x14, 0x12, 0x118, 0x13, colour byte). The original pushes
// each byte in a register whose upper bits are its caller's (eax zero there
// through the dispatch's `xor eax, eax`, ecx not); ours zero-extends. Group
// Y's 0x575690 masks it (`and eax, 0xFF` at 0x575718).
void LoadWindows() {
    g.window_back(static_cast<std::uint32_t>(FieldS_Backdrop));
    g.window_frame5(0x14, 0x12, 0x118, 0x13, static_cast<std::uint32_t>(static_cast<unsigned char>(BattleDraw_Colour)));
}
// Text_DrawAt(0x1C, 0x15, 0, 0xFF, system message id).
void LoadMessage(U id) { g.text_draw(0x1C, 0x15, 0, 0xFF, g.msg_ptr(id)); }

// `call File_LoadDone; test eax, eax; jne done; loop: Task_Sleep(1);
// File_LoadDone; je loop` - three times in 0x5885D0.
void WaitLoad() {
    if (g.load_done() != 0) return;
    do g.task_sleep(1);
    while (g.load_done() == 0);
}

}  // namespace

// The load menu's look for the quick menu (QM): the backdrop, the title box
// and a system message, as every load-menu state draws them first. Reads the
// Config's two bytes; writes only what the draws write.
void save_menu::DrawLoadWindows(unsigned message) {
    LoadWindows();
    LoadMessage(message);
}

// Every function below keeps its calls as calls: a tail jump to a function
// still Capcom's would hand it our caller's return address, which the frame
// hash records (docs/HANDOFF.md, Traps). The three dispatchers after the pop
// are the other way round: the originals jump, and so must ours.
#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ============================================================================
// Files
// ============================================================================

// 0x454770. Dat_FileNames[index] - the index a full dword, unchecked; a null
// name returns. The file is DAT\name, opened with File_Open (a failure
// returns), read whole into a Crt_malloc buffer (neither the read's count nor
// the buffer is tested), closed; then its 16-byte chunk headers walked - s8
// kind at +0, tag +4, size +8, payload +0x10 - and each kind-2 chunk handed to
// Snd_LoadBank(tag, payload, size): the sound banks alone, where LoadDatFile
// 0x454590 takes every kind. The size is read before the call. The walk runs
// while the offset is below the file's size, signed; the buffer is freed.
// Callers: the area and scene loaders (8 sites), for the banks of a scene.
//
// DIVERGENCE DIV-0089: when the cache is armed and holds DAT\name whole, its
// kind-2 chunks come from the cache's container instead, in the PC's order
// (dat_cache.h); the install's file is then not opened.
namespace {
void TakeBank(void*, const dat_cache::Chunk& c, std::uint8_t* payload) {
    if (c.kind == 2) g.load_bank(c.tag, payload, static_cast<U>(c.size));
}
}  // namespace
//
// DIVERGENCE DIV-0113: then each optional layer's DAT\<name> (BOF3X_OPT, by
// LoadDatFile's precedence), its kind-2 chunks to Snd_LoadBank too, so a
// layer's bank (psp-chord-one-tone) replaces the container's here as it does
// in LoadDatFile. No layer named: nothing more.
extern "C" void __cdecl Snd_LoadBankFile(unsigned index) {
    const U name = Long(at::kDatNames + index * 4);
    if (name == 0) return;
    const char* const text = reinterpret_cast<const char*>(At(name));
    if (dat_cache::WalkShipped(text, TakeBank, nullptr)) {  // DIV-0089
        DatLoad_OptLayerChunks(text, TakeBank);             // DIV-0113
        return;
    }
    char path[0x28];
    g.sprintf(path, reinterpret_cast<const char*>(At(at::kDatFormat)), reinterpret_cast<const char*>(At(name)));
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) return;
    const int size = g.file_size(handle);
    unsigned char* const data = static_cast<unsigned char*>(g.malloc(static_cast<unsigned>(size)));
    g.file_read(handle, data, static_cast<unsigned>(size));
    g.file_close(handle);
    if (size > 0) {
        int offset = 0;
        do {
            const U chunk = Addr(data) + static_cast<U>(offset);
            const U bytes = Long(chunk + 8);
            if (Byte(chunk) == 2) g.load_bank(Long(chunk + 4), At(chunk + 0x10), bytes);
            offset = static_cast<int>(static_cast<U>(offset) + bytes + 0x10);
        } while (offset < size);
    }
    g.free(data);
    DatLoad_OptLayerChunks(text, TakeBank);  // DIV-0113
}

// 0x454820 (docs/save-files.md). File_Open(path, 0, 0), -1 on failure;
// File_Seek(offset); File_Read(size) into Save_Staging; File_Close; 0. The
// seek's and the read's results are not looked at.
extern "C" int __cdecl Save_ReadFile(const char* path, unsigned size, int offset) {
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) return -1;
    g.file_seek(handle, offset);
    g.file_read(handle, At(Save_Staging_addr), size);
    g.file_close(handle);
    return 0;
}

// The quicksave round's draft B (docs/quicksave.md, section QM): the
// listing and the summary read take their pattern and their tables, so the
// quick menu (quick_menu.cpp) lists BISLPS1?.DAT into tables of its own; the
// original's callers pass Save_Directory and the summaries at 0x905BC0, with
// BISLPS0?.DAT for the original's BISLPS??.DAT - a quicksave's name[7] is a
// hex digit too, so BISLPS10.DAT would read as save slot 0 - and the bound
// the original never had: sixteen entries, the rest ignored and logged (a
// seventeenth match wrote past Save_Directory towards Save_Staging). Both
// switched on by SaveLists_Arm, after the fuzz has compared Capcom's
// behaviour; BOF3X_ORIGINAL=SaveLists keeps the original's pattern and its
// unbounded table.
namespace {
volatile unsigned char g_lists_split = 0;   // PatchBytes "SaveLists" sets it (SaveLists_Arm)
constexpr const char* kSavePattern = "BISLPS0?.DAT";
}  // namespace

// 0x4548B0's body (docs/save-files.md). The table's 0x60 dwords zeroed; the
// pattern searched with _findfirst into a _finddata_t on the stack (name at
// +0x14, size at +0x10), 0 when nothing matches. Per match its name - strlen
// + 1 bytes - to entry +0 and its size to +0x14, entries 0x18 apart, until
// _findnext answers non-zero; the count is returned. Unbounded (the
// original): no bound on the table or on a name. Bounded (draft B): a
// seventeenth match and every one after it, and a name of 0x14 bytes or more
// with its NUL (it would run into the entry's size), are not stored, each
// logged. The search handle is closed only when `close` (the quick menu's);
// the original's callers keep the original's leak.
int save_menu::ListFiles(const char* pattern, unsigned char* table, bool bounded, bool close) {
    const U base = Addr(table);
    for (U i = 0; i < 0x60; ++i) PutLong(base + i * 4, 0);
    unsigned char found[0x118];
    const long handle = g.find_first(pattern, found);
    if (handle == -1) return 0;
    int count = 0;
    U entry = base;
    do {
        const U name = Addr(found) + 0x14;
        U n = 0;
        while (At(name)[n] != 0) ++n;
        if (bounded && static_cast<U>(count) == kListEntries) {
            bof3::Log("save list   %s: %s is past the table's sixteen entries, not listed (draft B's bound)", pattern,
                      reinterpret_cast<const char*>(At(name)));
            continue;
        }
        if (bounded && n + 1 > 0x14) {
            bof3::Log("save list   %s: %s is %u bytes, over an entry's 0x14 for a name; not listed (draft B's bound)",
                      pattern, reinterpret_cast<const char*>(At(name)), static_cast<unsigned>(n + 1));
            continue;
        }
        CopyForward(entry, name, n + 1);
        U size;
        std::memcpy(&size, found + 0x10, sizeof size);
        rv::As<records::SaveDirEntry>(At(entry))->size = size;
        entry += at::kDirectoryBytes;
        ++count;
    } while (g.find_next(handle, found) == 0);
    if (close) _findclose(static_cast<intptr_t>(handle));
    return count;
}

// 0x4548B0 (docs/save-files.md): the body above on Save_Directory - with the
// original's BISLPS??.DAT (0x65289C) and no bound, or, armed, draft B's
// BISLPS0?.DAT and the bound. Kept either way, the original's defect: no
// _findclose (a search handle leaks per call).
extern "C" int __cdecl Save_ListFiles(void) {
    if (g_lists_split != 0) return save_menu::ListFiles(kSavePattern, At(at::kDirectory), true, false);
    return save_menu::ListFiles(reinterpret_cast<const char*>(At(at::kListPattern)), At(at::kDirectory), false, false);
}

// 0x588DC0's body (docs/save-files.md). All sixteen summaries marked empty
// (+0x15 = 0xFF); then for each slot 0..15 the first table entry whose name
// byte 7 is that slot's hex digit (0-9, A-F, a-f) is read - `read(its name,
// the summary)` - and the entry's index stored at +0x15. A read that answers
// false leaves the slot empty (the quick menu's reader; the original's never
// does).
void save_menu::ReadSummaries(const unsigned char* table, unsigned char* summaries, SummaryRead read, void* context) {
    const U dir = Addr(table), sums = Addr(summaries);
    for (U a = sums + 0x15; a < sums + 16 * at::kSummaryBytes + 0x15; a += at::kSummaryBytes) PutByte(a, 0xFF);
    for (U slot = 0; slot < 16; ++slot) {
        const U summary = sums + slot * at::kSummaryBytes;
        for (U n = 0; n < 16; ++n) {
            const U c = Byte(dir + n * at::kDirectoryBytes + 7);
            U v;
            if (c >= 0x30 && c <= 0x39) v = c - 0x30;
            else if (c >= 0x41 && c <= 0x46) v = c - 0x37;
            else if (c >= 0x61 && c <= 0x66) v = c - 0x57;
            else continue;
            if (v != slot) continue;
            if (read(rv::As<records::SaveDirEntry>(At(dir + n * at::kDirectoryBytes))->name, At(summary), context))
                rv::As<records::SaveSummary>(At(summary))->dir_index = n;
            break;
        }
    }
}

namespace {
// The original's read: Save_ReadFile(name, 0x1C, 0xCA0), its result not
// tested, and Save_Staging's first 0x1C bytes copied into the summary.
bool StagedRead(const char* name, unsigned char* summary, void*) {
    g.read_file(name, at::kSummaryBytes, static_cast<int>(at::kSummaryAt));
    for (U i = 0; i < 7; ++i) PutLong(Addr(summary) + i * 4, Long(at::kStaging + i * 4));
    return true;
}
}  // namespace

// 0x588DC0 (docs/save-files.md): the body above on Save_Directory and the
// summaries at 0x905BC0, through Save_ReadFile and Save_Staging. Kept: the
// read's answer is not tested, so a file that will not open leaves the
// summary whatever Save_Staging held. It rebuilds from Save_Directory alone:
// DIV-0002's re-listing in Save_WriteFile is what keeps a new save in it.
extern "C" void __cdecl Save_ReadSummaries(void) {
    save_menu::ReadSummaries(At(at::kDirectory), At(at::kSummaries), &StagedRead, nullptr);
}

void SaveLists_Arm() {
    static const std::uint8_t was = 0, is = 1;
    const bool on = bof3::PatchBytes("SaveLists", Addr(&g_lists_split), &was, &is, 1);
    bof3::Log("SaveLists   the save lists split (quicksave draft B): %s",
              on ? "the saves listed as BISLPS0?.DAT, sixteen at most" : "off (BOF3X_ORIGINAL): BISLPS??.DAT, unbounded");
}

// ============================================================================
// Small helpers
// ============================================================================

// 0x4549F0 (PSX 0x8014E294). Row (row & 0xFF) of the CLUT strip - 0x100
// words - copied from the source 0x4000 bytes below it, word by word, and
// Gfx_ClutStripDirty set; the row is not bounded (256 rows reach 0x20 KB past
// the strip). Its sibling 0x4549B0 (Gfx_ClutStripRestore) copies the whole
// strip and does not set the flag.
extern "C" void __cdecl Gfx_ClutStripCopyRow(unsigned row) {
    const U base = ((row & 0xFF) << 9) + Gfx_ClutStrip_addr;
    for (U i = 0; i < 0x100; ++i) PutWord(base + i * 2, Word(base + i * 2 - 0x4000));
    Gfx_ClutStripDirty = 1;
}

// 0x5A7020. 0 without Music_Buffer; else IDirectSoundBuffer::GetStatus into a
// local that nothing initialises - the slot the entry's `push ecx` made - and
// bit 0 (DSBSTATUS_PLAYING) of its low byte. A GetStatus that fails without
// writing leaves the caller's ecx to be tested, so the naked entry passes it.
// Callers Sound_StreamDone and the tail jump at 0x587C20.
extern "C" int __cdecl SaveMenu_MusicPlayingBody(U ecx) {
    void* const buffer = static_cast<unsigned char*>(Music_Buffer);
    if (buffer == nullptr) return 0;
    U status = ecx;
    void* const* vtable;
    std::memcpy(&vtable, buffer, sizeof vtable);
    reinterpret_cast<long (__stdcall*)(void*, U*)>(vtable[0x24 / 4])(buffer, &status);
    return (status & 1) ? 1 : 0;
}

// 0x587910 (PSX op 89's 0x80164090 streams from the CD; this is the port's
// own). kind = id >> 12 of the whole dword, kept in 0x6BDE40; the name is
// entry id & 0xFFF of table [0x6653B0 + 4 kind] - neither checked; the file
// SND\name.DAT opened (a failure returns with the kind already stored). The
// previous stream's buffer is freed, the file read whole into a new one
// (0x6BDE44, stored before the read), closed. Kind non-zero: 0x5A7140 plays it
// (the buffer re-read) and that is all. Kind 0: a fade in progress
// (Music_FadeCount) is dropped - count 0, Music_Stop, Music_Track 0xFF - and
// Music_Start(buffer, size, 0) plays it once at Music_SetVolume(127). The id
// is a dword: one caller (0x446E8B) pushes ecx & 0xFFFF + 0x1000.
//
// DIVERGENCE DIV-0089: when the cache is armed and has base\snd\name.DAT, that
// file is the one opened (docs/sound-import.md section 7); SND\name.DAT
// otherwise, formatted as before. The path buffer is MAX_PATH for the
// cache's absolute path (the original's 0x28 holds every SND\ name).
//
// DIVERGENCE DIV-0129: a failed open is remembered (g_stream_missing, ours;
// logic-visible, so the quicksave's SOUND record carries it, sound.cpp) until
// the next open succeeds, and Sound_StreamDone answers
// 1 meanwhile - the original's wait went on asking about the area's song,
// which the failed load never stopped, so the inn and the camp hung on an
// install without the jingle (D255).
namespace {
bool g_stream_missing = false;
}
void save_menu::StreamMissingClear() { g_stream_missing = false; }
bool save_menu::StreamMissing() { return g_stream_missing; }                  // the quicksave (sound.cpp)
void save_menu::SetStreamMissing(bool missing) { g_stream_missing = missing; }
extern "C" void __cdecl Sound_LoadStream(unsigned id) {
    const U kind = id >> 12;
    SaveMenu_StreamKind = kind;
    const U table = Long(at::kStreamTables + kind * 4);
    const U name = Long(table + (id & 0xFFF) * 4);
    char path[260];  // MAX_PATH
    if (!dat_cache::SoundPath(reinterpret_cast<const char*>(At(name)), path, sizeof path))  // DIV-0089
        g.sprintf(path, reinterpret_cast<const char*>(At(at::kStreamFormat)), reinterpret_cast<const char*>(At(name)));
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) {
        if (!g_stream_missing)
            bof3::Log("sound       DIV-0129: %s is not there; a wait on it ends at once", path);
        g_stream_missing = true;  // DIV-0129
        return;
    }
    g_stream_missing = false;
    if (SaveMenu_StreamData != 0) g.free(At(SaveMenu_StreamData));
    const int size = g.file_size(handle);
    void* const data = g.malloc(static_cast<unsigned>(size));
    SaveMenu_StreamData = Addr(data);
    g.file_read(handle, data, static_cast<unsigned>(size));
    g.file_close(handle);
    if (SaveMenu_StreamKind != 0) {
        g.voice_play(At(SaveMenu_StreamData));
        bof3::RunSpeed_StreamStarted();
        return;
    }
    if (static_cast<std::uint32_t>(Music_FadeCount) != 0) {
        Music_FadeCount = 0;
        g.music_stop();
        Music_Track = 0xFF;
    }
    g.music_start(At(SaveMenu_StreamData), static_cast<unsigned>(size), 0);
    g.music_volume(at::kStreamVolume);
    bof3::RunSpeed_StreamStarted();
}

// 0x587A00 (PSX 0x80164890 reads its stream's byte). 1 once the last stream
// has stopped: kind non-zero asks 0x5A7200, kind 0 Music_IsPlaying, and the
// answer's bit 0 is flipped (`xor eax, 1` on the whole register). Op 8B and
// the inn wait on it.
//
// DIVERGENCE DIV-0129: 1 while the last stream's file could not be opened.
extern "C" int __cdecl Sound_StreamDone(void) {
    if (g_stream_missing) return 1;  // DIV-0129
    if (SaveMenu_StreamKind != 0) return g.voice_playing() ^ 1;
    return g.music_playing() ^ 1;
}

// hook/run_speed.h: whether the stream Sound_LoadStream last started is still
// playing - Sound_StreamDone's question, asked by WinMain's loop under
// BOF3X_SPEED. The flag is ours (no game state) and drops once the stream
// has stopped, so a stream nobody waits on costs a few status reads.
namespace {
bool g_stream_started = false;
}
void bof3::RunSpeed_StreamStarted() { g_stream_started = true; }
void bof3::RunSpeed_StreamForget() { g_stream_started = false; }
bool bof3::RunSpeed_StreamPlaying() {
    if (!g_stream_started) return false;
    if (Sound_StreamDone() != 0) g_stream_started = false;
    return g_stream_started;
}

// ============================================================================
// The inn and the save menu's pieces
// ============================================================================

// 0x580630 (PSX 0x801D68E0, the 'callers' tier). For each of the eight
// character records: with `full` (a byte) the HP scale +0x1E cleared,
// Char_ClearStatus(i, 0xA0, 0) and +0x1C = +0x2E; then +0x1D cleared,
// Char_RecalcStats, HP +0x18 = max +0x20 and AP +0x1A = max +0x22, +0x1D
// cleared again, Char_ClearStatus(i, 0x2000, 0). Then, when Flags_Test(0x904654,
// 4) and not Flags_Test(0x904657, 4), the byte 0x9045FB is cleared if it is
// below 0x1E. The inn (0x57FCD5) passes 1; 0x5805DC passes 0. The original's
// id dword is its loop byte over the entry's ecx; Char_ClearStatus reads the
// byte.
extern "C" void __cdecl Party_RestoreAll(unsigned full) {
    for (U i = 0; i < 8; ++i) {
        const U record = at::kCharRecords + i * at::kCharStride;
        if ((full & 0xFF) != 0) {
            PutByte(record + 0x1E, 0);
            g.clear_status(i, 0xA0, 0);
            PutByte(record + 0x1C, Byte(record + 0x2E));
        }
        PutByte(record + 0x1D, 0);
        g.recalc(At(record));
        const U hp = Word(record + 0x20), ap = Word(record + 0x22);
        PutWord(record + 0x18, hp);
        PutWord(record + 0x1A, ap);
        PutByte(record + 0x1D, 0);
        g.clear_status(i, 0x2000, 0);
    }
    if (g.flags_test(At(at::kFlagsA), 4) == 0) return;
    if (g.flags_test(At(at::kFlagsB), 4) != 0) return;
    if (static_cast<std::uint32_t>(AreaW1c_Byte9045FB) < 0x1E) AreaW1c_Byte9045FB = 0;
}

// 0x5808E0 (PSX 0x801D6D7C). The window 0x57CF60(x + 4, y + 4, 0x52, 0x48, 0,
// colour byte) and 0x5762D0(x, y, 9, 8); then system messages 0xD2..0xD4 by
// Text_DrawAt at x + 0xC, one 20 px row each from y + 0xB, and Menu_DrawHand
// at x + 0xC on the row of the cursor byte 0x6BC881, y + 0xD. Kept, two
// register leftovers: the row index is `movzx si, bl` over x, so each text y
// has 20 * (x & 0xFFFF0000) in it (nothing for any x from 0 to 0xFFFF); and
// the hand's row is `movzx ax, byte`, over the upper half of the last
// Text_DrawAt's result. Both vanish in a 16-bit y.
extern "C" void __cdecl SaveMenu_DrawChoices(int x, int y) {
    g.window_frame(x + 4, y + 4, 0x52, 0x48, 0, static_cast<std::uint32_t>(static_cast<unsigned char>(BattleDraw_Colour)));
    g.window_box(x, y, 9, 8);
    const U high = static_cast<U>(x) & 0xFFFF0000u;
    U last = 0;
    for (U i = 0; i < 3; ++i) {
        const U row = high | i;
        last = Addr(g.text_draw(x + 0xC, static_cast<int>(static_cast<U>(y) + row * 20 + 0xB), 0, 0xFF,
                                g.msg_ptr(row + 0xD2)));
    }
    const U hand = (last & 0xFFFF0000u) | static_cast<std::uint32_t>(ShopStates_Choice);
    g.draw_hand(x + 0xC, static_cast<int>(static_cast<U>(y) + hand * 20 + 0xD), 0);
}

// 0x580970 (PSX 0x801DF800). One TILE at Gfx_PacketNext: Gpu_SetTile,
// Gpu_SetSemiTrans(0), then (0, 0), 320 x 240 as floats and colour 0 - stored
// after the two calls - and Gfx_CommitPrim(1, 0x1C): the screen black behind
// the inn's night.
extern "C" void __cdecl Menu_DrawBlackScreen(void) {
    unsigned char* const p = Packet();
    g.set_tile(p);
    g.set_semi(p, 0);
    auto* const t = rv::As<records::GpuTile>(p);   // a TILE: Gpu_SetTile's packet
    const float wide = static_cast<float>(Widescreen_Live());   // DIV-0041: the black covers a wide picture whole
    t->x = 0.0f - wide;   // not -wide: that is -0.0f when wide is 0
    t->y = 0.0f;
    t->w = 320.0f + 2 * wide;
    t->h = 240.0f;
    t->r = 0;
    t->g = 0;
    t->b = 0;
    g.commit(1, 0x1C);
}

// ============================================================================
// The shop
// ============================================================================

// 0x583020 (PSX 0x801D3570, 'callers'). *rate = 100; for each party member i
// (Party_Count(0), asked again after each, its byte compared unsigned) whose
// record - Field_Members' byte - has item 0x1B in slot +0x16 or +0x17: 80,
// and no further member. Then 0x5918E0(1) non-zero with byte 0x929EC3 6 or 7:
// 70. The loop counter is the low byte of the argument's own stack slot,
// read back whole and masked; it cannot show.
extern "C" void __cdecl Shop_PriceRate(unsigned short* rate) {
    *rate = 0x64;
    U i = 0;
    if (static_cast<unsigned char>(g.party_count(0)) != 0) {
        for (;;) {
            const U record = Byte(at::kMembers + i * at::kMemberStride);
            const U base = at::kCharRecords + record * at::kCharStride;
            const auto* const c = rv::AsConst<records::CharRecord>(At(base));   // CharacterRecords 0x903A70, stride 0xA4
            if (c->accessory[0] == at::kDiscountItem || c->accessory[1] == at::kDiscountItem) {
                *rate = 0x50;
                break;
            }
            i = (i + 1) & 0xFF;
            if (i >= static_cast<unsigned char>(g.party_count(0))) break;
        }
    }
    if (g.shop_flag(1) == 0) return;
    const U window = ScenaSc1_ShopByteB;
    if (window == 6 || window == 7) *rate = 0x46;
}

// 0x5830D0 (PSX 0x801D3664). price * (rate & 0xFFFF), 32 bits, divided by
// 100 unsigned (the multiply-by-0x51EB851F shift-37 form, exact for every
// dword); 1 in place of 0.
extern "C" unsigned __cdecl Shop_ScalePrice(unsigned price, unsigned rate) {
    const U product = (rate & 0xFFFF) * price;
    const U scaled = product / 100;
    return scaled != 0 ? scaled : 1;
}

// 0x583100. 0x5749F0(kind, item) & 0xFFFF; with the flag byte set, kind byte
// 0 and item byte 0x2B..0x36 that times 50, otherwise half.
extern "C" unsigned __cdecl Shop_SellPrice(unsigned kind, unsigned item, unsigned flag) {
    const U price = g.item_price(kind, item) & 0xFFFF;
    const U i = item & 0xFF;
    if ((flag & 0xFF) != 0 && (kind & 0xFF) == 0 && i >= 0x2B && i <= 0x36) return price * 50;
    return price >> 1;
}

// 0x583140. The shop's windows set up: fixed bytes and words into the
// window records at 0x803160, 0x803184, 0x8031A8, 0x8031B2 and 0x803430 /
// 0x803454, in the original's order.
extern "C" void __cdecl Shop_InitWindows(void) {
    // Window records 0, 1, 2, 20 and 21 of WindowRecords 0x803160, 0x24 apart
    // (symbols.toml Window_Alloc): their shared bytes +0..+7 typed; record 0's
    // +0x10 is the entry ShopStates2_Help; records 2's and 20's +0xA / +0xB are
    // the handler's own (WindowRecord's per-handler bytes), kept raw.
    auto* const w0 = rv::As<records::WindowRecord>(At(bof3::addr::WindowRecords + 0 * sizeof(records::WindowRecord)));
    auto* const w1 = rv::As<records::WindowRecord>(At(bof3::addr::WindowRecords + 1 * sizeof(records::WindowRecord)));
    auto* const w2 = rv::As<records::WindowRecord>(At(bof3::addr::WindowRecords + 2 * sizeof(records::WindowRecord)));
    auto* const w20 = rv::As<records::WindowRecord>(At(bof3::addr::WindowRecords + 20 * sizeof(records::WindowRecord)));
    auto* const w21 = rv::As<records::WindowRecord>(At(bof3::addr::WindowRecords + 21 * sizeof(records::WindowRecord)));
    w0->state = 2;
    w1->state = 2;
    w2->kind = 2;
    w2->state = 2;
    PutByte(0x8031B2, 3);
    w21->kind = 3;
    w0->handler = 7;
    w0->kind = 0;
    w0->in_use = 1;
    w0->x = 0x14;
    w0->y = 0xFFEC;
    ShopStates2_Help = 0;
    w1->handler = 7;
    w1->kind = 1;
    w1->in_use = 1;
    w1->x = 0xC6;
    w1->y = 0xFFEC;
    w2->handler = 7;
    w2->in_use = 1;
    w2->x = 0x66;
    w2->y = 0xFFEC;
    PutByte(0x8031B3, 0xFF);
    w21->handler = 7;
    w21->in_use = 0;
    w20->handler = 7;
    w20->kind = 4;
    w20->in_use = 0;
    PutByte(0x80343A, 0);
    PutByte(0x80343B, 1);
}

// 0x583210. Equips `item` in slot (slot & 0xFF) of record (record & 0xFF):
// slots 1..5 are bytes +0x13..+0x17 (kinds 2, 2, 2, 3, 3), any other slot
// byte - 0 or 6 and up, the switch's default - is +0x12 (kind 1). Per slot:
// 0x591B60(kind, item, 1, 0), then Inventory_Add(kind, the slot's old byte,
// read after that call, 1, 0), then the item's low byte into the slot.
extern "C" void __cdecl Shop_Equip(unsigned record, unsigned slot, unsigned item) {
    const U base = at::kCharRecords + (record & 0xFF) * at::kCharStride;
    U offset = 0x12, kind = 1;
    switch ((slot & 0xFF) - 1) {
    case 0: offset = 0x13; kind = 2; break;
    case 1: offset = 0x14; kind = 2; break;
    case 2: offset = 0x15; kind = 2; break;
    case 3: offset = 0x16; kind = 3; break;
    case 4: offset = 0x17; kind = 3; break;
    default: break;
    }
    g.inventory_remove(kind, item, 1, 0);
    g.inventory_add(kind, Byte(base + offset), 1, 0);
    PutByte(base + offset, item);
}

// ============================================================================
// The title and load menus' draws
// ============================================================================

// 0x588880 (docs/title-menu.md section 2). With something to load, rows 0, 1,
// 2 at y 0x50, 0x70, 0x90; otherwise rows 0 and 2 at 0x60, 0x80. `glow` is
// passed through whole.
extern "C" void __cdecl TitleMenu_DrawRows(unsigned glow) {
    if (SaveMenu_AnySave != 0) {
        g.draw_row(0x50, 0, glow);
        g.draw_row(0x70, 1, glow);
        g.draw_row(0x90, 2, glow);
        return;
    }
    g.draw_row(0x60, 0, glow);
    g.draw_row(0x80, 2, glow);
}

// 0x5888D0 (docs/title-menu.md section 2). Row `row` (a byte) of the title
// sheet, 32 tall from v = row << 5 (a byte), centred at x = 0xA0 - w / 2,
// y = (s16) y, its width w one of three bytes. With `glow` (a byte) a glow
// SPRT goes first: draw mode page Gpu_GetTPage(0, 2, 0x380, 0) & 0xFFFF,
// CLUT Gpu_GetClut(0, 0x1EB), colour twice the row's brightness (a byte
// shift). Then the row itself: Gpu_GetTPage(0, 1, 0x380, 0), CLUT (0x20,
// 0x1E3), the brightness as it is. Both sprites get Gpu_SetSemiTrans(glow) and
// Gfx_CommitPrim(1, 0x1C) after a draw mode's (1, 0xC); Gfx_PacketNext is
// read again after each commit, and the brightness byte again for each
// channel. The draw mode's last argument is the fifth dword Gpu_GetTPage's
// call left on the stack, a 0.
//
// The widths are the immediates of the original's three `mov byte [esp +
// 0x1x], w` (0x5888E4..0x5888F2), as title_menu.cpp holds them: DIV-0014
// writes the English widths there, and this is where they still take effect
// with the code ours (docs/operand-store.md).
extern "C" void __cdecl TitleMenu_DrawRow(int y, unsigned row, unsigned glow) {
    const U r = row & 0xFF;
    const U width = TitleMenu_Width(r < 3 ? r : 0);   // at::kRowWidths[r], as title_menu.cpp holds it
    // A row past 2 reads the stack beyond the three bytes in the original:
    // nothing calls it so, and ours refuses rather than invent a width.
    if (r >= 3) bof3::Fatal("TitleMenu_DrawRow: row %u, the original reads past its width table", (unsigned)r);
    const float fx = static_cast<float>(static_cast<int>(0xA0 - (width >> 1)));
    const float fy = static_cast<float>(static_cast<short>(y));
    if ((glow & 0xFF) != 0) {
        const U glow_page = g.get_tpage(0, 2, 0x380, 0) & 0xFFFF;   // before Gfx_PacketNext is read
        g.set_draw_mode(Packet(), 0, 0, glow_page, 0);
        g.commit(1, 0xC);
        unsigned char* const p = Packet();
        auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
        g.set_sprt(p);
        s->u = 0;
        s->h = 0x20;
        s->x = fx;
        s->y = fy;
        rv::Store<U>(s->v, r << 5);
        rv::Store<U>(s->w, width);
        rv::Store<U>(s->clut, g.get_clut(0, 0x1EB));
        rv::Store<U>(s->r, Byte(at::kRowBright + r) << 1);
        rv::Store<U>(s->g, Byte(at::kRowBright + r) << 1);
        rv::Store<U>(s->b, Byte(at::kRowBright + r) << 1);
        g.set_semi(p, glow & 0xFF);
        g.commit(1, 0x1C);
    }
    const U page = g.get_tpage(0, 1, 0x380, 0) & 0xFFFF;
    g.set_draw_mode(Packet(), 0, 0, page, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Packet();
    auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
    g.set_sprt(p);
    s->u = 0;
    rv::Store<U>(s->v, r << 5);
    s->h = 0x20;
    s->x = fx;
    s->y = fy;
    rv::Store<U>(s->w, width);
    rv::Store<U>(s->clut, g.get_clut(0x20, 0x1E3));
    rv::Store<U>(s->r, Byte(at::kRowBright + r));
    rv::Store<U>(s->g, Byte(at::kRowBright + r));
    rv::Store<U>(s->b, Byte(at::kRowBright + r));
    g.set_semi(p, glow & 0xFF);
    g.commit(1, 0x1C);
}

// 0x588C90. One SPRT at Gfx_PacketNext: colour 0x20 on all three channels,
// (s16 x, s16 y) as floats, u / v / w / h / CLUT from record (index & 0xFF)
// of the 10-byte table 0x66726C (u and v as bytes, the CLUT word shifted left
// 6), then Gfx_CommitPrim(slot, 0x1C) - slot the whole dword.
extern "C" void __cdecl TitleMenu_DrawPiece(int x, int y, unsigned index, unsigned slot) {
    unsigned char* const p = Packet();
    auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
    const U q = Addr(p);
    g.set_sprt(p);
    s->r = 0x20;
    s->g = 0x20;
    s->b = 0x20;
    // The two floats by address: typed, the compiler pairs them into one
    // vector store (codegen_same).
    PutFloat(q + 8, static_cast<float>(static_cast<short>(x)));
    PutFloat(q + 0xC, static_cast<float>(static_cast<short>(y)));
    const U e = at::kPieces + (index & 0xFF) * 10;
    rv::Store<U>(s->u, Byte(e));
    rv::Store<U>(s->v, Byte(e + 2));
    rv::Store<U>(s->w, Word(e + 4));
    rv::Store<U>(s->h, Word(e + 6));
    rv::Store<U>(s->clut, Word(e + 8) << 6);
    g.commit(slot, 0x1C);
}

// 0x588C00. The title menu's frame: draw mode page 0xBB, the pieces 0 at
// (0x1A, 0x18) and 1 at (0x10A, 0x88); draw mode page 0xB9, the pieces 2 at
// (-6, 0x1C) and 3 at (0xDA, 0x1C); every one on slot 2.
extern "C" void __cdecl TitleMenu_DrawFrame(void) {
    g.set_draw_mode(Packet(), 0, 0, 0xBB, 0);
    g.commit(2, 0xC);
    g.draw_piece(0x1A, 0x18, 0, 2);
    g.draw_piece(0x10A, 0x88, 1, 2);
    g.set_draw_mode(Packet(), 0, 0, 0xB9, 0);
    g.commit(2, 0xC);
    g.draw_piece(-6, 0x1C, 2, 2);
    g.draw_piece(0xDA, 0x1C, 3, 2);
}

// 0x588D20 (PSX 0x801D67C0 / 0x801DDF6C, call-anchored). Three save slots
// from the first shown, 0x8034D0: 0x576960(slot, x + 0x10, y + 56 i + 6, its
// summary or 0 when +0x15 is 0xFF) - the slot argument is the first slot's
// dword with only its low byte advanced (`add cl, bl`) - then the cursor
// 0x573CE0(x + 0x10, y + 6 + 56 (cursor - first), 0xD1, 0x34, highlight, 6).
// The first slot and the cursor are read again for each.
// The body takes the summaries and the two words (the quicksave round's QM:
// the quick menu passes its own listing, first-shown and cursor - ours, not
// the game's - and the original's callers reach it through the entry below).
void save_menu::DrawSlotsOver(int x, int y, unsigned highlight, const unsigned char* summaries,
                              const volatile U* top_word, const volatile U* cursor_word) {
    const int left = x + 0x10;
    for (U i = 0; i < 3; ++i) {
        const U first = *top_word;
        const U n = first + i;
        const U summary = Addr(summaries) + n * at::kSummaryBytes;
        const unsigned char* const s = rv::As<records::SaveSummary>(At(summary))->dir_index != 0xFF ? At(summary) : nullptr;
        const U slot = (first & 0xFFFFFF00u) | ((first + i) & 0xFF);
        g.slot_draw(slot, left, static_cast<int>(static_cast<U>(y) + i * 56 + 6), s);
    }
    const U first = *top_word;
    const U cursor = *cursor_word;
    g.slot_cursor(left, static_cast<int>((cursor * 56 - first * 56) + static_cast<U>(y) + 6), 0xD1, 0x34, highlight, 6);
}

extern "C" void __cdecl SaveMenu_DrawSlots(int x, int y, unsigned highlight) {
    save_menu::DrawSlotsOver(x, y, highlight, At(at::kSummaries), reinterpret_cast<const volatile U*>(At(at::kSlotTop)),
                             reinterpret_cast<const volatile U*>(At(Rest2c_Slot_addr)));
}

// ============================================================================
// The growth rolls of 0x588800
// ============================================================================

// 0x588BA0 (PSX 0x801E70DC). The sum, as a byte, of n (a byte) values
// Rand() % (hi - lo + 1) + lo - the remainder of a signed division by the
// difference of the two bytes plus one, its low byte added to lo's. No check:
// hi = lo - 1 divides by zero as the original does.
extern "C" unsigned char __cdecl NewGame_RollSum(unsigned lo, unsigned hi, unsigned n) {
    unsigned char sum = 0;
    const int span = static_cast<int>(hi & 0xFF) - static_cast<int>(lo & 0xFF) + 1;
    for (U k = n & 0xFF; k != 0; --k) {
        const int r = g.rand() % span;
        sum = static_cast<unsigned char>(sum + static_cast<unsigned char>(r + static_cast<int>(lo & 0xFF)));
    }
    return sum;
}

// 0x588AC0 (PSX 0x801E7190). For character (member & 0xFF), from its three
// bytes {lo, hi, n} at 0x667248: five NewGame_RollSum, the first four
// results mapped through 0x667260 into 0x903640 + 5 member + 0..3; then all
// five bytes cleared. Nothing survives but the Rand calls; the PSX twin
// does exactly the same, the fifth result stored and cleared too.
extern "C" void __cdecl NewGame_RollGrowth(unsigned member) {
    const U m = member & 0xFF;
    const U lo = Byte(at::kGrowthTable + m * 3), hi = Byte(at::kGrowthTable + m * 3 + 1),
            n = Byte(at::kGrowthTable + m * 3 + 2);
    const U out = at::kGrowth + m * 5;
    for (U k = 0; k < 4; ++k) PutByte(out + k, Byte(at::kGrowthValues + g.roll_sum(lo, hi, n)));
    g.roll_sum(lo, hi, n);
    for (U k = 0; k < 5; ++k) PutByte(out + k, 0);
}

// ============================================================================
// The title and load flow - task 0's mode 1
// ============================================================================

// 0x587DC0, Game_Step 0 (pointer-reached, 0x6671F4[0]). The field made ready
// for the title's backdrop: both script flag words and the party set byte
// cleared; Field_State and Sprite_Current = ObjTrio; Field_MemberSprite(0,
// 0); the sprite's +0 = 1, Field_State +0x89 = 0, +0x2E = 0x10A, +0x30 = 0x51,
// +0x29 and +0x48 = 0, +9 = 1, +0x24 = 0x81, +6 = 4, +9 = 0 (each through
// Sprite_Current read again), and Sprite_SetAnimation(+6 + 8). Then the party
// set byte 0xFF, Save_ListFiles, Game_Step + 1, 0x9039D6 and the state 0, and
// the answer's non-zero into both 0x6BDF8E (three rows) and the cursor - the
// cursor starts on LOAD GAME when saves exist.
extern "C" void __cdecl TitleFlow_Begin(void) {
    Field_ScriptFlags = 0;
    Field_ScriptFlags2 = 0;
    AreaW0b_PartySet = 0;
    PutLong(at::kFieldState, ObjTrio_addr);
    PutLong(at::kSpriteCurrent, ObjTrio_addr);
    g.member_sprite(0, 0);
    rv::Current<records::SpriteObject>()->flags = 1;
    rv::As<records::PartyMember>(Field_State)->char_id = 0;
    rv::Current<records::SpriteObject>()->screen_x = 0x10A;
    rv::Current<records::SpriteObject>()->screen_y = 0x51;
    rv::Current<records::SpriteObject>()->ot_slot = 0;
    rv::Current<records::SpriteObject>()->scale_mode = 0;
    rv::Current<records::SpriteObject>()->step_frames = 1;
    rv::Current<records::SpriteObject>()->draw_flags = 0x81;
    rv::Current<records::SpriteObject>()->type = 4;
    rv::Current<records::SpriteObject>()->step_frames = 0;
    g.set_animation(static_cast<unsigned char>(static_cast<U>(rv::Current<records::SpriteObject>()->type) + 8));
    AreaW0b_PartySet = 0xFF;
    const U any = g.list_files() != 0 ? 1 : 0;
    Game_Step = static_cast<std::uint32_t>(Game_Step) + 1;
    SaveMenu_Flow9039D6 = 0;
    SetState(0);
    SaveMenu_AnySave = any;
    SaveMenu_Cursor = any;
}

// 0x587EB0, title menu state 0: the frame, counter 2, state + 1.
extern "C" void __cdecl TitleMenu_Open(void) {
    g.draw_frame();
    PutByte(at::kCounter, 2);
    SetState(State() + 1);
}

// 0x587ED0, state 1: the frame; state + 1 when the counter, decremented,
// reaches 0.
extern "C" void __cdecl TitleMenu_OpenWait(void) {
    g.draw_frame();
    const U c = (Byte(at::kCounter) - 1) & 0xFF;
    PutByte(at::kCounter, c);
    if (c == 0) SetState(State() + 1);
}

// 0x587EF0, state 2: the frame, bit 1 of 0x7DEE44, state + 1.
extern "C" void __cdecl TitleMenu_FrameUp(void) {
    g.draw_frame();
    PutByte(at::kMenuFlags, Byte(at::kMenuFlags) | 2);
    SetState(State() + 1);
}

// 0x587F10, state 3: the counter and all three row brightnesses (and the
// byte after them - a dword store) 0; the frame, the rows with glow, state + 1.
extern "C" void __cdecl TitleMenu_RowsStart(void) {
    PutByte(at::kCounter, 0);
    PutLong(at::kRowBright, 0);
    g.draw_frame();
    g.draw_rows(1);
    SetState(State() + 1);
}

// 0x587F40, state 4: the frame, the rows with glow; the counter + 0x10;
// every row brightness + 8, held at 0x40 (unsigned) except the cursor's row,
// which rises unheld (it wraps past 0xFF); state + 1 when the counter comes
// round to 0 - sixteen frames.
extern "C" void __cdecl TitleMenu_RowsFadeIn(void) {
    g.draw_frame();
    g.draw_rows(1);
    PutByte(at::kCounter, Byte(at::kCounter) + 0x10);
    const int cursor = SaveMenu_Cursor;
    for (int i = 0; i < 3; ++i) {
        const U b = (Byte(at::kRowBright + i) + 8) & 0xFF;
        PutByte(at::kRowBright + i, cursor != i && b > 0x40 ? 0x40 : b);
    }
    if (Byte(at::kCounter) == 0) SetState(State() + 1);
}

// 0x587FA0, state 5: the frame and the rows without glow; nothing more until
// bit 1 of 0x7DEE44. With something to load, up (0x1000) and down (0x4000) of
// Input_Pressed move the cursor over 0..2, each wrapping - both may apply in
// one frame; the down test is signed, so a cursor at 0x7F goes to -0x80 and
// stays - otherwise either toggles it between 0 and 2. The cursor's row
// brightens by 8 to at most 0x80, the others dim by 8 to at least 0x40
// (unsigned). A confirm button, or Start (0x800): Transition_Start(0), state
// + 1 (read after it), Sound_PlayEffect(0x105).
extern "C" void __cdecl TitleMenu_Choose(void) {
    g.draw_frame();
    g.draw_rows(0);
    if ((Byte(at::kMenuFlags) & 2) == 0) return;
    const U pressed = Long(at::kPressed);
    if (SaveMenu_AnySave != 0) {
        if (pressed & 0x1000) {
            const signed char c = static_cast<signed char>(static_cast<std::uint32_t>(static_cast<unsigned char>(SaveMenu_Cursor)) - 1);
            SaveMenu_Cursor = static_cast<unsigned char>(c);
            if (c < 0) SaveMenu_Cursor = 2;
        }
        if (pressed & 0x4000) {
            const signed char c = static_cast<signed char>(static_cast<std::uint32_t>(static_cast<unsigned char>(SaveMenu_Cursor)) + 1);
            SaveMenu_Cursor = static_cast<unsigned char>(c);
            if (c > 2) SaveMenu_Cursor = 0;
        }
    } else if (pressed & 0x5000) {
        SaveMenu_Cursor = static_cast<std::uint32_t>(static_cast<unsigned char>(SaveMenu_Cursor)) ^ 2;
    }
    const int cursor = SaveMenu_Cursor;
    for (int i = 0; i < 3; ++i) {
        const U a = at::kRowBright + static_cast<U>(i);
        if (cursor == i) {
            PutByte(a, Byte(a) + 8);
            if (Byte(a) > 0x80) PutByte(a, 0x80);
        } else {
            PutByte(a, Byte(a) + 0xF8);
            if (Byte(a) < 0x40) PutByte(a, 0x40);
        }
    }
    if ((static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) == 0 && (pressed & 0x800) == 0) return;
    g.transition(0);
    SetState(State() + 1);
    g.play_effect(0x105);
}

// 0x5880A0, state 6: once no transition runs (the wait word 0), Game_Step =
// cursor + 2 - the new game, the load, or the config screen - and state 0;
// until then the frame and the rows.
extern "C" void __cdecl TitleMenu_Leave(void) {
    if (MoveScript_WaitWordDA == 0) {
        const U step = static_cast<U>(SaveMenu_Cursor + 2);
        SetState(0);
        Game_Step = step;
        return;
    }
    g.draw_frame();
    g.draw_rows(0);
}

// 0x5880E0, Game_Step 2, the new game. NewGame_InitCharacters; 0x9039F2
// cleared; Game_AreaNumber 0xFFFF; Scenario_Start(0); the first dword of
// every 8 bytes of Cond_Flags up to 0x904030 cleared - half of the 0xA0
// bytes, as the original's stride has it; Transition_Start(0); Game_Step 5.
extern "C" void __cdecl TitleFlow_NewGame(void) {
    g.init_characters();
    SaveMenu_NewGameByte = 0;
    Game_AreaNumber = 0xFFFF;
    g.scenario_start(0);
    for (U a = Cond_Flags_addr; a < at::kCondFlagsEnd; a += 8) PutLong(a, 0);
    g.transition(0);
    Game_Step = 5;
}

// 0x588140, load menu state 0: Transition_Start(1); the cursor slot and the
// first shown 0; Save_ListFiles - with files Save_ReadSummaries and state
// + 1, without Sound_PlayEffect(0x107), error 1 and state 5.
extern "C" void __cdecl LoadMenu_List(void) {
    g.transition(1);
    Rest2c_Slot = 0;
    PutLong(at::kSlotTop, 0);
    if (g.list_files() != 0) {
        g.read_summaries();
        SetState(State() + 1);
        return;
    }
    g.play_effect(0x107);
    SaveMenu_ErrorKind = 1;
    SetState(5);
}

// 0x588190, state 1: the windows, message 0xAC, the slots with the cursor
// lit; state + 1 once no transition runs.
extern "C" void __cdecl LoadMenu_Open(void) {
    LoadWindows();
    LoadMessage(0xAC);
    g.draw_slots(0x20, 0x30, 1);
    if (MoveScript_WaitWordDA == 0) SetState(State() + 1);
}

// 0x5881F0, state 2, choosing a slot: the same draw; then Input_AutoRepeat
// of Input_Pressed & 0x5000. Up (0x1000) with the slot above 0: slot - 1,
// and the first shown follows it up when the slot drops below it (signed).
// Otherwise down (0x4000) with the slot below 15 (signed): slot + 1, and the
// first shown + 1 (read again) when the slot passes first + 2. A slot whose
// value differs from the old one's low byte: Sound_PlayEffect(0x101). Confirm
// on a slot whose summary is present: 0x104, 0x929F0B = 1, state + 1; on an
// empty one 0x107. Else cancel: 0x106, state 0, Game_Step 1 - the title menu.
extern "C" void __cdecl LoadMenu_Choose(void) {
    LoadWindows();
    LoadMessage(0xAC);
    g.draw_slots(0x20, 0x30, 1);
    const U repeat = g.auto_repeat(static_cast<std::uint32_t>(Input_Pressed) & 0x5000);
    U slot = Long(at::kSlot);
    const U old = slot & 0xFF;
    if ((repeat & 0x1000) && slot != 0) {
        --slot;
        Rest2c_Slot = slot;
        const U top = Long(at::kSlotTop);
        if (static_cast<int>(slot) < static_cast<int>(top)) PutLong(at::kSlotTop, top - 1);
    } else if ((repeat & 0x4000) && static_cast<int>(slot) < 15) {
        ++slot;
        Rest2c_Slot = slot;
        if (static_cast<int>(slot) > static_cast<int>(Long(at::kSlotTop) + 2)) PutLong(at::kSlotTop, Long(at::kSlotTop) + 1);
    }
    if (old != slot) {
        g.play_effect(0x101);
        slot = Rest2c_Slot;
    }
    const U pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        if (Byte(at::kSummaries + slot * at::kSummaryBytes + 0x15) != 0xFF) {
            g.play_effect(0x104);
            const U s = State();
            FieldS_Answer = 1;
            SetState(s + 1);
            return;
        }
        g.play_effect(0x107);
        return;
    }
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        g.play_effect(0x106);
        SetState(0);
        Game_Step = 1;
    }
}

// 0x588340, state 3: the windows, message 0xB9 (load this game?), the slots
// unlit; Menu_YesNo - on an answer, state + 1 for yes (0x929F0B), - 1 for no.
extern "C" void __cdecl LoadMenu_Confirm(void) {
    LoadWindows();
    LoadMessage(0xB9);
    g.draw_slots(0x20, 0x30, 0);
    if (g.yes_no() == 0) return;
    if (static_cast<unsigned char>(FieldS_Answer) != 0) SetState(State() + 1);
    else SetState(State() - 1);
}

// 0x5883C0, state 4, the read: the windows and the slots; 0x1C00 bytes of
// Save_Staging zeroed; BISLPS<slot>.DAT formatted into 0x904BA0 and read
// whole (0x12B0 from 0). A failed read: error 2, state 5, no sound. Else the
// checksum word (+0x70) is taken - as a dword - and zeroed, the 0x10B0 bytes
// copied to the game block while summed; Field_ScriptFlags = the block's
// byte +0x74D (after the copy, whatever the sum); the sum's low word equal
// to the checksum's: state 6; otherwise Sound_PlayEffect(0x107), error 2,
// state 5 - with the game block already overwritten.
extern "C" void __cdecl LoadMenu_Read(void) {
    LoadWindows();
    g.draw_slots(0x20, 0x30, 0);
    const U slot = Rest2c_Slot;
    for (U i = 0; i < at::kClearBytes; i += 4) PutLong(at::kStaging + i, 0);
    g.sprintf(reinterpret_cast<char*>(At(at::kPath)), reinterpret_cast<const char*>(At(at::kSaveName)), slot);
    if (g.read_file(reinterpret_cast<const char*>(At(at::kPath)), at::kFileBytes, 0) == -1) {
        SaveMenu_ErrorKind = 2;
        SetState(5);
        return;
    }
    const U stored = Long(at::kChecksum);
    PutWord(at::kChecksum, 0);
    U sum = 0;
    for (U i = 0; i < at::kBlockBytes; ++i) {
        const U c = Byte(at::kStaging + i);
        PutByte(at::kGameBlock + i, c);
        sum += c;
    }
    Field_ScriptFlags = static_cast<std::uint32_t>(SaveMenu_ScriptFlagsSaved);
    if ((sum & 0xFFFF) == (stored & 0xFFFF)) {
        // Instrumentation, not behaviour: tools/input_run.py hands slot 0 back
        // on this line (docs/input-script.md section 1a) - the file is not
        // read again until a save menu lists the slots.
        bof3::Log("save        loaded slot %X (%s)", (unsigned)slot, reinterpret_cast<const char*>(At(at::kPath)));
        SetState(6);
        return;
    }
    g.play_effect(0x107);
    SaveMenu_ErrorKind = 2;
    SetState(5);
}

// 0x5884A0, state 5, the error: the windows; error 1 - message 0x11 + 0xA6
// (no save), then back to the title menu (state 0, Game_Step 1); error 2 -
// 0x12 + 0xA6 (could not load), then back to choosing (state 2, Game_Step 3).
// Confirm leaves: Sound_PlayEffect(0x104) and the two stores. Any other error
// value keeps the last message byte and takes both targets from a local the
// original never sets - the high byte of the ecx its caller left, which the
// naked entry passes in. Only 1 and 2 are ever stored.
extern "C" void __cdecl SaveMenu_LoadErrorBody(U ecx) {
    U local = ecx >> 24, step;
    LoadWindows();
    const U kind = SaveMenu_ErrorKind;
    if (kind == 1) {
        SaveMenu_ErrorText = 0x11;
        step = 1;
        local = 0;
    } else if (kind == 2) {
        SaveMenu_ErrorText = 0x12;
        step = 3;
        local = 2;
    } else {
        step = local;
    }
    LoadMessage(static_cast<std::uint32_t>(SaveMenu_ErrorText) + 0xA6);
    if ((static_cast<std::uint32_t>(Field_ConfirmButtons) & static_cast<std::uint32_t>(Input_Pressed)) == 0) return;
    g.play_effect(0x104);
    SetState(local);
    PutWord(at::kGameStep, step & 0xFF);
}

// 0x588560, state 6, loaded: the windows, the slots, message 0xB1;
// Transition_Start(0); state + 1 (read after it); 30 frames before the game
// is applied.
extern "C" void __cdecl LoadMenu_Loaded(void) {
    LoadWindows();
    g.draw_slots(0x20, 0x30, 0);
    LoadMessage(0xB1);
    g.transition(0);
    const U s = State();
    SaveMenu_ApplyDelay = 0x1E;
    SetState(s + 1);
}

// 0x5885D0, state 7: the countdown, decremented as a byte; while it was not
// yet 0 only the draw at the end. At 0, the loaded game is applied: the pad
// map from the game block (0x90469C.. to 0x903580.., 0x12 bytes, by dwords
// and a word) - or, when Start was held during the countdown (0x6BDF96), the
// default map; Field_MemberCount = Party_Count(0); PartySet_Select(party set
// & 0x7F, 3) and a wait for the load, the same with 0 and a wait;
// Field_PartyLoad(0); the block's first 16 bytes to 0x8034E0; 0x929ED0 =
// Cond_Flags + 8 * (the block's first byte, signed); the four counters from
// block +0x20; Scenario_Load and a wait; Game_Step 5 and state 0. Then, in
// either case, while a transition runs: Input_Held's Start sets 0x6BDF96, and
// the windows, the slots and message 0xB1 are drawn. The waits are Task_Sleep
// loops on File_LoadDone, which is always 1 on the PC.
extern "C" void __cdecl LoadMenu_Apply(void) {
    const U delay = SaveMenu_ApplyDelay;
    SaveMenu_ApplyDelay = delay - 1;
    if (delay == 0) {
        const U map_4 = Long(at::kSavedButtons + 4), map_0 = SaveMenu_SavedButtons, map_8 = Long(at::kSavedButtons + 8);
        PutLong(at::kButtons + 4, map_4);
        const U map_10 = Word(at::kSavedButtons + 0x10);
        PutLong(at::kButtons, map_0);
        const U map_c = Long(at::kSavedButtons + 0xC);
        PutWord(at::kButtons + 0x10, map_10);
        PutLong(at::kButtons + 8, map_8);
        PutLong(at::kButtons + 0xC, map_c);
        const bool reset = SaveMenu_DefaultPad != 0;
        if (reset) {
            PutWord(at::kButtons, 0x20);
            PutWord(at::kButtons + 2, 0x80);
            PutWord(at::kButtons + 0xC, 0x40);
            PutWord(at::kButtons + 4, 0x10);
            PutWord(at::kButtons + 6, 8);
            PutWord(at::kButtons + 8, 4);
            PutWord(at::kButtons + 0xE, 0x20);
            PutWord(at::kButtons + 0x10, 0x40);
            PutWord(at::kButtons + 0xA, 0x100);
        }
        // DIV-0133 and DIV-0134 (docs/button-sigma.md): the Start-held reset
        // by source, then words 7 / 8 as Speak's and Action's and the table
        // (a save whose six lines are not one button each refused) - nothing
        // until sigma::Arm, after every module's self-test.
        bof3x::input::sigma::LoadApplied(reset);
        Field_MemberCount = static_cast<U>(g.party_count(0));
        g.party_select(static_cast<std::uint32_t>(AreaW0b_PartySet) & 0x7F, 3);
        WaitLoad();
        g.party_select(static_cast<std::uint32_t>(AreaW0b_PartySet) & 0x7F, 0);
        WaitLoad();
        g.party_load(0);
        const U d8 = Long(at::kGameBlock + 8), d0 = Long(at::kGameBlock), d4 = Long(at::kGameBlock + 4);
        PutLong(at::kPosition + 8, d8);
        PutLong(at::kPosition + 4, d4);
        const U dc = Long(at::kGameBlock + 0xC);
        PutLong(at::kPosition, d0);
        PutLong(at::kPosition + 0xC, dc);
        const U c0 = Byte(at::kGameBlock + 0x20);
        const U cond = Cond_Flags_addr + static_cast<U>(static_cast<int>(static_cast<signed char>(d0 & 0xFF)) * 8);
        const U c1 = Byte(at::kGameBlock + 0x21);
        AreaW0c_FlagRow = cond;
        const U c2 = Byte(at::kGameBlock + 0x22);
        PutByte(at::kCounters, c0);
        const U c3 = Byte(at::kGameBlock + 0x23);
        PutByte(at::kCounters + 1, c1);
        PutByte(at::kCounters + 2, c2);
        PutByte(at::kCounters + 3, c3);
        g.scenario_load();
        WaitLoad();
        Game_Step = 5;
        SetState(0);
    }
    if (MoveScript_WaitWordDA == 0) return;
    if (Long(at::kHeld) & 0x800) SaveMenu_DefaultPad = 1;
    LoadWindows();
    g.draw_slots(0x20, 0x30, 0);
    LoadMessage(0xB1);
}

// 0x588800, Game_Step 5 (PSX 0x801E6228, 'callers'): nothing while a
// transition runs. Then 0x929F02, 0x929F03, 0x929F05 cleared; the cursor on
// row 2 (config) goes back to the title menu - state 0, Game_Step 1. Else
// for each of the eight characters NewGame_RollGrowth and Char_RecalcStats
// of its record - after a load too - and task 0 restarted at 0x495800, the
// game's body (0x5A9976, which does not return).
extern "C" void __cdecl TitleFlow_EnterGame(void) {
    if (MoveScript_WaitWordDA != 0) return;
    const U cursor = static_cast<unsigned char>(SaveMenu_Cursor);
    FieldS_Step = 0;
    PutByte(at::kMenuStates + 1, 0);
    PutByte(at::kMenuStates + 3, 0);
    if (cursor == 2) {
        SetState(0);
        Game_Step = 1;
        return;
    }
    for (U i = 0; i < 8; ++i) {
        g.roll_growth(i);
        g.recalc(At(at::kCharRecords + i * at::kCharStride));
    }
    g.task_restart(at::kGameTask);
}

#pragma clang attribute pop

// The two entries whose originals read the ecx their caller left, through
// an uninitialised local: 0x5A7020 (Music_IsPlaying) and 0x5884A0
// (LoadMenu_Error). Each passes ecx to its body, above, as it found it.
extern "C" __attribute__((naked)) int __cdecl Music_IsPlaying(void) {
    __asm__ volatile(
        "push %ecx\n\t"
        "call _SaveMenu_MusicPlayingBody\n\t"
        "add $4, %esp\n\t"
        "ret");
}
extern "C" __attribute__((naked)) void __cdecl LoadMenu_Error(void) {
    __asm__ volatile(
        "push %ecx\n\t"
        "call _SaveMenu_LoadErrorBody\n\t"
        "add $4, %esp\n\t"
        "ret");
}

// 0x587DB0, task 0's mode 1: `jmp [0x6671F4 + 4 Game_Step]`, the word
// unchecked. Steps 0 TitleFlow_Begin, 1 the title menu, 2 the new game, 3 the
// load menu, 4 the config screen ConfigScreen_Run 0x460CB0, 5 TitleFlow_EnterGame.
// A jump, as the original's: the step's function returns to task 0's loop.
extern "C" void __cdecl TitleFlow_Step(void) {
    using Step = void (__cdecl*)();
    const Step step = reinterpret_cast<Step>(At(Long(at::kStepTable + static_cast<std::uint32_t>(Game_Step) * 4)));
    [[clang::musttail]] return step();
}

// 0x587EA0, Game_Step 1: `jmp [0x66720C + 4 state]` - the title menu's
// states 0..6 (the table runs on into the load menu's).
extern "C" void __cdecl TitleFlow_Menu(void) {
    using Step = void (__cdecl*)();
    const Step step = reinterpret_cast<Step>(At(Long(at::kMenuTable + State() * 4)));
    [[clang::musttail]] return step();
}

// 0x588130, Game_Step 3: `jmp [0x667228 + 4 state]` - the load menu's states
// 0..7.
extern "C" void __cdecl TitleFlow_Load(void) {
    using Step = void (__cdecl*)();
    const Step step = reinterpret_cast<Step>(At(Long(at::kLoadTable + State() * 4)));
    [[clang::musttail]] return step();
}

// --- BOF3X_SHADOW=dat_cache: DIV-0089 through the two sound readers -----------
//
// Sound_LoadStream and Snd_LoadBankFile on dat_cache's in-memory cache, with
// File_Open and Snd_LoadBank stood in by recorders (the open fails, so the
// stream reader returns before it plays anything): which file each opens,
// and which banks, in what order. A stream name and a container name from the
// game's own tables; no file of the game's is read.
namespace {
std::vector<std::string>* g_sound_opens;
std::vector<U>* g_sound_banks;
int __cdecl RecordOpen(const char* path, int, int) {
    g_sound_opens->push_back(path);
    return -1;
}
void __cdecl RecordBank(unsigned tag, const void*, unsigned) { g_sound_banks->push_back(tag); }

void CacheSoundSelfTest() {
    const U table = Long(at::kStreamTables + 1 * 4);  // kind 1: the SND\ waves
    if (table == 0 || Long(table) == 0) bof3::Fatal("DIV-0089 self-test: no stream name of kind 1");
    const std::string stream = reinterpret_cast<const char*>(At(Long(table)));
    unsigned index = 0;
    while (index < Dat_FileNames_count && !Dat_FileNames[index]) ++index;
    if (index == Dat_FileNames_count) bof3::Fatal("DIV-0089 self-test: Dat_FileNames is empty");
    const std::string bank = Dat_FileNames[index];
    const std::string root = "T:\\bof3x-dat-selftest";
    using dat_cache::TestContainer;
    std::vector<dat_cache::TestFile> files = {
        {root + "\\base\\snd\\" + stream + ".DAT", {1, 2, 3, 4}},
        {root + "\\base\\dat\\" + bank, TestContainer({{2, 2, 16, 1}, {2, 5, 16, 2}})},
        {root + "\\loc\\zh-CN\\dat\\" + bank, TestContainer({{0, 0x4000, 8, 3}})}};
    const std::string hash(64, 'a');
    const std::string manifest = "[meta]\ntarget = \"pc-zh\"\n\n[cache]\nassets = [\n"
                                 "  [\"" + bank + "\", 0, \"base\", \"pc-zh:chunk\", \"" + hash + "\"],\n"
                                 "  [\"" + bank + "\", 1, \"loc/zh-CN\", \"pc-zh:chunk\", \"" + hash + "\"],\n"
                                 "  [\"" + bank + "\", 2, \"base\", \"pc-zh:chunk\", \"" + hash + "\"],\n]\n";
    std::vector<std::string> cache_opens;
    char error[512];
    if (!dat_cache::TestBegin(root.c_str(), manifest.c_str(), dat_cache::TestIo(&files, &cache_opens), error,
                              sizeof error))
        bof3::Fatal("DIV-0089 self-test: the sound readers' cache refused: %s", error);

    const Callees saved = g;
    const U kind = SaveMenu_StreamKind;
    const bool missing = g_stream_missing;  // DIV-0129's flag: the opens below fail
    std::vector<std::string> opens;
    std::vector<U> banks;
    g_sound_opens = &opens;
    g_sound_banks = &banks;
    g.file_open = RecordOpen;
    g.load_bank = RecordBank;
    const std::string cache_wave = root + "\\base\\snd\\" + stream + ".DAT", install_wave = "SND\\" + stream + ".DAT";

    using Opens = std::vector<std::string>;
    using Banks = std::vector<U>;
    // Each check is a predicate, so that its control - the output a bug of
    // that kind would give - goes through the very same test.
    const auto from_cache = [&](const Opens& o) { return o.size() == 1 && o[0] == cache_wave; };
    const auto from_install = [&](const Opens& o) { return o.size() == 1 && o[0] == install_wave; };
    const auto banks_from_cache = [&](const Banks& b, const Opens& o) {
        return b.size() == 2 && b[0] == 2 && b[1] == 5 && o.size() == 1;
    };
    const auto as_original = [&](const Opens& o, const Banks& b) {
        return o.size() == 2 && o[0] == install_wave && o[1] == "DAT\\" + bank && b.empty();
    };

    Sound_LoadStream(0x1000);  // armed, the cache has the wave
    const bool wave_cache = from_cache(opens);
    opens.clear();
    files[0].path = root + "\\base\\snd\\OTHER.DAT";  // armed, the cache lacks it
    Sound_LoadStream(0x1000);
    const bool wave_lacks = from_install(opens);
    files[0].path = cache_wave;
    Snd_LoadBankFile(index);  // armed: the cache's banks in slot order, no install file opened
    const bool banks_cache = banks_from_cache(banks, opens);
    dat_cache::TestSetArmed(false);
    opens.clear();
    banks.clear();
    Sound_LoadStream(0x1000);  // not armed: SND\, as the original
    Snd_LoadBankFile(index);   // not armed: DAT\, as the original
    const bool unarmed = as_original(opens, banks);
    // The controls: an install-first stream reader (SND\ though the cache has
    // the wave), a cache-first one that ignores a missing file, a bank reader
    // that also opens the install's DAT\ file, an unarmed reader that reads
    // the cache - each refused by its check.
    unsigned refused = 0;
    refused += !from_cache({install_wave});
    refused += !from_install({cache_wave});
    refused += !banks_from_cache({2, 5}, {install_wave, "DAT\\" + bank});
    refused += !as_original({cache_wave, "DAT\\" + bank}, {});

    g = saved;
    SaveMenu_StreamKind = kind;
    g_stream_missing = missing;
    g_sound_opens = nullptr;
    g_sound_banks = nullptr;
    dat_cache::TestEnd();
    if (!wave_cache || !wave_lacks || !banks_cache || !unarmed || refused != 4)
        bof3::Fatal("DIV-0089 self-test: Sound_LoadStream from the cache %s, the cache lacking it %s; Snd_LoadBankFile "
                    "from the cache %s; unarmed %s; controls refused %u of 4",
                    wave_cache ? "ok" : "WRONG", wave_lacks ? "ok" : "WRONG", banks_cache ? "ok" : "WRONG",
                    unarmed ? "ok" : "WRONG", refused);
    bof3::Log("shadow      DIV-0089 self-test: Sound_LoadStream(0x1000) opens %s from the cache, SND\\ without it or "
              "unarmed; Snd_LoadBankFile(%u) %s's banks from the cache, DAT\\ unarmed; 4 controls refused",
              stream.c_str(), index, bank.c_str());
}

// --- BOF3X_SHADOW=save_menu: DIV-0129, a missing stream ends its wait --------
//
// Sound_LoadStream(0) - kind 0, the inn's jingle - with File_Open failing and
// the area's song still playing: Sound_StreamDone must answer 1, where the
// original rule (Music_IsPlaying ^ 1) answers 0 and the inn waits for good;
// and with the stream there (the flag clear) the song's answer, 0, as before.
int __cdecl FailOpen(const char*, int, int) { return -1; }
int __cdecl StillPlaying() { return 1; }

void StreamMissingSelfTest() {
    const Callees saved = g;
    const U kind = SaveMenu_StreamKind;
    const bool missing = g_stream_missing;
    g.file_open = FailOpen;
    g.music_playing = StillPlaying;
    g.voice_playing = StillPlaying;
    g_stream_missing = false;
    Sound_LoadStream(0);
    const int failed = Sound_StreamDone();
    const int original = g.music_playing() ^ 1;  // the control: the original's answer to the same state
    g_stream_missing = false;                    // as after an open that succeeded
    const int present = Sound_StreamDone();
    g = saved;
    SaveMenu_StreamKind = kind;
    g_stream_missing = missing;
    if (failed != 1 || original != 0 || present != 0)
        bof3::Fatal("DIV-0129 self-test: a missing stream's wait answers %d (want 1), the original's %d (want 0), a "
                    "present stream's %d (want 0)", failed, original, present);
    bof3::Log("shadow      DIV-0129 self-test: a missing kind-0 stream ends its wait (1; the original 0, the hang); "
              "a present one waits on its song (0)");
}
}  // namespace

void SaveMenu_Inject() {
    TitleMenu_Operands();   // DIV-0014's widths, which our TitleMenu_DrawRow reads, against their immediates
    if (bof3::WantsShadow("dat_cache")) CacheSoundSelfTest();
    if (bof3::WantsShadow("save_menu")) StreamMissingSelfTest();
    if (bof3::WantsShadow("save_menu")) save_menu::SelfTest();
    save_menu::StreamMissingClear();  // DIV-0129: the game starts as no failed open (the fuzz's opens fail)
    BOF3_INJECT(Snd_LoadBankFile);
    BOF3_INJECT(Save_ReadFile);
    BOF3_INJECT(Save_ListFiles);
    BOF3_INJECT(Gfx_ClutStripCopyRow);
    BOF3_INJECT(Party_RestoreAll);
    BOF3_INJECT(SaveMenu_DrawChoices);
    BOF3_INJECT(Menu_DrawBlackScreen);
    BOF3_INJECT(Shop_PriceRate);
    BOF3_INJECT(Shop_ScalePrice);
    BOF3_INJECT(Shop_SellPrice);
    BOF3_INJECT(Shop_InitWindows);
    BOF3_INJECT(Shop_Equip);
    BOF3_INJECT(TitleMenu_DrawRows);
    BOF3_INJECT(TitleMenu_DrawRow);
    BOF3_INJECT(NewGame_RollGrowth);
    BOF3_INJECT(NewGame_RollSum);
    BOF3_INJECT(TitleMenu_DrawFrame);
    BOF3_INJECT(TitleMenu_DrawPiece);
    BOF3_INJECT(SaveMenu_DrawSlots);
    BOF3_INJECT(Save_ReadSummaries);
    BOF3_INJECT(Sound_LoadStream);
    BOF3_INJECT(Sound_StreamDone);
    BOF3_INJECT(Music_IsPlaying);
    BOF3_INJECT(TitleFlow_Step);
    BOF3_INJECT(TitleFlow_Begin);
    BOF3_INJECT(TitleFlow_Menu);
    BOF3_INJECT(TitleMenu_Open);
    BOF3_INJECT(TitleMenu_OpenWait);
    BOF3_INJECT(TitleMenu_FrameUp);
    BOF3_INJECT(TitleMenu_RowsStart);
    BOF3_INJECT(TitleMenu_RowsFadeIn);
    BOF3_INJECT(TitleMenu_Choose);
    BOF3_INJECT(TitleMenu_Leave);
    BOF3_INJECT(TitleFlow_NewGame);
    BOF3_INJECT(TitleFlow_Load);
    BOF3_INJECT(LoadMenu_List);
    BOF3_INJECT(LoadMenu_Open);
    BOF3_INJECT(LoadMenu_Choose);
    BOF3_INJECT(LoadMenu_Confirm);
    BOF3_INJECT(LoadMenu_Read);
    BOF3_INJECT(LoadMenu_Error);
    BOF3_INJECT(LoadMenu_Loaded);
    BOF3_INJECT(LoadMenu_Apply);
    BOF3_INJECT(TitleFlow_EnterGame);
}
