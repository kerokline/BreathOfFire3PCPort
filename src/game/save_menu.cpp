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
#include "common/asm_name.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/music_seq.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/save_menu_callees.h"
#include "game/sound_in.h"
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
    Fn<void (__cdecl*)(const void*)>(::bof3::Code(kVoicePlay.v)), Fn<int (__cdecl*)()>(::bof3::Code(kVoiceIsPlaying.v)),
    Char_ClearStatus,
    Fn<void (__cdecl*)(unsigned char*)>(::bof3::Code(kRecalcStats.v)),
    Flags_Test, Party_Count,
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemPrice.v)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(kShopFlag.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(kInventoryRemove.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(kInventoryAdd.v)),
    Fn<void (__cdecl*)(int, int, int, int, int, unsigned)>(::bof3::Code(kWindowFrame.v)),
    Fn<void (__cdecl*)(int, int, int, int)>(::bof3::Code(kWindowBox.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kWindowBack.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned)>(::bof3::Code(kWindowFrame5.v)),
    Fn<void (__cdecl*)(unsigned, int, int, const unsigned char*)>(::bof3::Code(kSlotDraw.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, int)>(::bof3::Code(kSlotCursor.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(kMenuYesNo.v)),
    Msg_SystemPtr, Text_DrawAt,
    Fn<void (__cdecl*)(int, int, int)>(::bof3::Code(kDrawHand.v)),
    Input_AutoRepeat,
    Gpu_SetTile, Gpu_SetSprt, Gpu_SetSemiTrans, Gpu_SetDrawMode, Gpu_GetTPage, Gpu_GetClut, Gfx_CommitPrim,
    Transition_Start, Field_MemberSprite, Sprite_SetAnimation, NewGame_InitCharacters, Scenario_Start,
    Scenario_Load, Field_PartyLoad, PartySet_Select, File_LoadDone, Task_Sleep,
    Fn<void (__cdecl*)(U)>(::bof3::Code(kTaskRestart.v)),
    Save_ListFiles, Save_ReadFile, Save_ReadSummaries,
    TitleMenu_DrawFrame, TitleMenu_DrawRows, TitleMenu_DrawRow, TitleMenu_DrawPiece, SaveMenu_DrawSlots,
    NewGame_RollGrowth, NewGame_RollSum,
};
Callees g = kOriginals;

}  // namespace save_menu

using namespace save_menu;

namespace {

unsigned char* AtSaveMenu(save_menu::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
save_menu::U AddrSaveMenu(const volatile void* p) { return Low32(p); }
save_menu::U Byte(save_menu::U address) { return AtSaveMenu(address)[0]; }
void PutByte(save_menu::U address, save_menu::U v) { AtSaveMenu(address)[0] = static_cast<unsigned char>(v); }
save_menu::U WordSaveMenu(save_menu::U address) {
    std::uint16_t v;
    std::memcpy(&v, AtSaveMenu(address), sizeof v);
    return v;
}
void PutWord(save_menu::U address, save_menu::U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(AtSaveMenu(address), &w, sizeof w);
}
save_menu::U LongSaveMenu(save_menu::U address) {
    save_menu::U v;
    std::memcpy(&v, AtSaveMenu(address), sizeof v);
    return v;
}
void PutLongSaveMenu(save_menu::U address, save_menu::U v) { std::memcpy(AtSaveMenu(address), &v, sizeof v); }

// The record views (game/record_views.h) used here: a save
// summary (0x905BC0, 16 of 0x1C), a Save_Directory entry (0x929F40, 16 of
// 0x18), Sprite_Current read afresh (below +0x80, the sprite head every
// family shares) and Field_State, the leader's party record.

unsigned char* Packet() { return Gfx_PacketNext; }

void SetState(save_menu::U v) { SaveMenu_FlowState = v; }

// `rep movsd` then `rep movsb`, forward, each byte read just before it is
// stored.
void CopyForward(U dst, U src, U n) {
    for (U i = 0; i < n; ++i) AtSaveMenu(dst)[i] = AtSaveMenu(src)[i];
}

// The window pair every load-menu state draws first: 0x575690 with the style
// byte, 0x574AB0(0x14, 0x12, 0x118, 0x13, colour byte). The original pushes
// each byte in a register whose upper bits are its caller's (eax zero there
// through the dispatch's `xor eax, eax`, ecx not); ours zero-extends. Group
// Y's 0x575690 masks it (`and eax, 0xFF` at 0x575718).
void LoadWindows() {
    save_menu::g.window_back(static_cast<std::uint32_t>(Config_Background));
    save_menu::g.window_frame5(0x14, 0x12, 0x118, 0x13, static_cast<std::uint32_t>(static_cast<unsigned char>(Config_WindowColour)));
}
// Text_DrawAt(0x1C, 0x15, 0, 0xFF, system message id).
void LoadMessage(save_menu::U id) { save_menu::g.text_draw(0x1C, 0x15, 0, 0xFF, save_menu::g.msg_ptr(id)); }

}  // namespace

// The load menu's look for the quick menu (QM): the backdrop (when
// `backdrop`), the title box and a message line, as every load-menu state
// draws them first. Reads the Config's two bytes; writes only what the draws
// write.
void save_menu::DrawLoadWindows(bool backdrop, const unsigned char* message) {
    if (backdrop) g.window_back(static_cast<std::uint32_t>(Config_Background));
    g.window_frame5(0x14, 0x12, 0x118, 0x13, static_cast<std::uint32_t>(static_cast<unsigned char>(Config_WindowColour)));
    g.text_draw(0x1C, 0x15, 0, 0xFF, message);
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
    if (c.kind == 2) save_menu::g.load_bank(c.tag, payload, static_cast<U>(c.size));
}
}  // namespace
//
// DIVERGENCE DIV-0113: then each optional layer's DAT\<name> (BOF3X_OPT, by
// LoadDatFile's precedence), its kind-2 chunks to Snd_LoadBank too, so a
// layer's bank (psp-chord-one-tone) replaces the container's here as it does
// in LoadDatFile. No layer named: nothing more.
extern "C" void __cdecl Snd_LoadBankFile(unsigned index) {
    const U name = LongSaveMenu(::bof3::Img(at::kDatNames.v) + index * 4);
    if (name == 0) return;
    const char* const text = reinterpret_cast<const char*>(AtSaveMenu(name));
    if (dat_cache::WalkShipped(text, TakeBank, nullptr)) {  // DIV-0089
        DatLoad_OptLayerChunks(text, TakeBank);             // DIV-0113
        return;
    }
    char path[0x28];
    save_menu::g.sprintf(path, reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(at::kDatFormat.v))), reinterpret_cast<const char*>(AtSaveMenu(name)));
    const int handle = save_menu::g.file_open(path, 0, 0);
    if (handle == -1) return;
    const int size = save_menu::g.file_size(handle);
    unsigned char* const data = static_cast<unsigned char*>(save_menu::g.malloc(static_cast<unsigned>(size)));
    save_menu::g.file_read(handle, data, static_cast<unsigned>(size));
    save_menu::g.file_close(handle);
    if (size > 0) {
        int offset = 0;
        do {
            const U chunk = AddrSaveMenu(data) + static_cast<U>(offset);
            const U bytes = LongSaveMenu(chunk + 8);
            if (Byte(chunk) == 2) save_menu::g.load_bank(LongSaveMenu(chunk + 4), AtSaveMenu(chunk + 0x10), bytes);
            offset = static_cast<int>(static_cast<U>(offset) + bytes + 0x10);
        } while (offset < size);
    }
    save_menu::g.free(data);
    DatLoad_OptLayerChunks(text, TakeBank);  // DIV-0113
}

// 0x454820 (docs/save-files.md). File_Open(path, 0, 0), -1 on failure;
// File_Seek(offset); File_Read(size) into Save_Staging; File_Close; 0. The
// seek's and the read's results are not looked at.
extern "C" int __cdecl Save_ReadFile(const char* path, unsigned size, int offset) {
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) return -1;
    g.file_seek(handle, offset);
    g.file_read(handle, AtSaveMenu(::bof3::Img(Save_Staging_addr)), size);
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

namespace save_menu { volatile unsigned char g_lists_split = 0; }  namespace {   // PatchBytes "SaveLists" sets it (SaveLists_Arm)
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
    const U base = AddrSaveMenu(table);
    for (U i = 0; i < 0x60; ++i) PutLongSaveMenu(base + i * 4, 0);
    unsigned char found[0x118];
    const long handle = g.find_first(pattern, found);
    if (handle == -1) return 0;
    int count = 0;
    U entry = base;
    do {
        const U name = AddrSaveMenu(found) + 0x14;
        U n = 0;
        while (AtSaveMenu(name)[n] != 0) ++n;
        if (bounded && static_cast<U>(count) == kListEntries) {
            bof3::Log("save list   %s: %s is past the table's sixteen entries, not listed (draft B's bound)", pattern,
                      reinterpret_cast<const char*>(AtSaveMenu(name)));
            continue;
        }
        if (bounded && n + 1 > 0x14) {
            bof3::Log("save list   %s: %s is %u bytes, over an entry's 0x14 for a name; not listed (draft B's bound)",
                      pattern, reinterpret_cast<const char*>(AtSaveMenu(name)), static_cast<unsigned>(n + 1));
            continue;
        }
        CopyForward(entry, name, n + 1);
        U size;
        std::memcpy(&size, found + 0x10, sizeof size);
        rv::As<records::SaveDirEntry>(AtSaveMenu(entry))->size = size;
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
    if (save_menu::g_lists_split != 0) return save_menu::ListFiles(kSavePattern, AtSaveMenu(::bof3::Img(Save_Directory_addr)), true, false);
    return save_menu::ListFiles(reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(at::kListPattern.v))), AtSaveMenu(::bof3::Img(Save_Directory_addr)), false, false);
}

// 0x588DC0's body (docs/save-files.md). All sixteen summaries marked empty
// (+0x15 = 0xFF); then for each slot 0..15 the first table entry whose name
// byte 7 is that slot's hex digit (0-9, A-F, a-f) is read - `read(its name,
// the summary)` - and the entry's index stored at +0x15. A read that answers
// false leaves the slot empty (the quick menu's reader; the original's never
// does).
void save_menu::ReadSummaries(const unsigned char* table, unsigned char* summaries, SummaryRead read, void* context) {
    const U dir = AddrSaveMenu(table), sums = AddrSaveMenu(summaries);
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
            if (read(rv::As<records::SaveDirEntry>(AtSaveMenu(dir + n * at::kDirectoryBytes))->name, AtSaveMenu(summary), context))
                rv::As<records::SaveSummary>(AtSaveMenu(summary))->dir_index = n;
            break;
        }
    }
}

void SaveLists_Arm() {
    static const std::uint8_t was = 0, is = 1;
    const bool on = bof3::PatchBytes("SaveLists", AddrSaveMenu(&save_menu::g_lists_split), &was, &is, 1);
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
    const U base = ((row & 0xFF) << 9) + ::bof3::Img(Gfx_ClutStrip_addr);
    for (U i = 0; i < 0x100; ++i) PutWord(base + i * 2, WordSaveMenu(base + i * 2 - 0x4000));
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
//
// DIVERGENCE DIV-0177 (sound_in.h): any stream forgets the last jingle's
// song; a kind-0 one, its file open, takes the song on the ring, halted or
// playing - before the dropped fade's Music_Stop and the jingle's
// Music_Start - to be started again at the game's Music_Resume (halted) or
// at the jingle's end (playing). DLL state only; with the switch off (the
// fuzz runs before it is armed) nothing but the forgetting.

namespace save_menu { bool g_stream_missing = false; }  namespace {
}
void save_menu::StreamMissingClear() { save_menu::g_stream_missing = false; }
bool save_menu::StreamMissing() { return save_menu::g_stream_missing; }                  // the quicksave (sound.cpp)
void save_menu::SetStreamMissing(bool missing) { save_menu::g_stream_missing = missing; }

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

// ============================================================================
// The shop
// ============================================================================

// ============================================================================
// The title and load menus' draws
// ============================================================================

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
        const U summary = AddrSaveMenu(summaries) + n * at::kSummaryBytes;
        const unsigned char* const s = rv::As<records::SaveSummary>(AtSaveMenu(summary))->dir_index != 0xFF ? AtSaveMenu(summary) : nullptr;
        const U slot = (first & 0xFFFFFF00u) | ((first + i) & 0xFF);
        g.slot_draw(slot, left, static_cast<int>(static_cast<U>(y) + i * 56 + 6), s);
    }
    const U first = *top_word;
    const U cursor = *cursor_word;
    g.slot_cursor(left, static_cast<int>((cursor * 56 - first * 56) + static_cast<U>(y) + 6), 0xD1, 0x34, highlight, 6);
}

// ============================================================================
// The growth rolls of 0x588800
// ============================================================================

// ============================================================================
// The title and load flow - task 0's mode 1
// ============================================================================

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
    PutWord(::bof3::Img(at::kGameStep.v), step & 0xFF);
}

#pragma clang attribute pop

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
    const U table = LongSaveMenu(::bof3::Img(at::kStreamTables.v) + 1 * 4);  // kind 1: the SND\ waves
    if (table == 0 || LongSaveMenu(table) == 0) bof3::Fatal("DIV-0089 self-test: no stream name of kind 1");
    const std::string stream = reinterpret_cast<const char*>(AtSaveMenu(LongSaveMenu(table)));
    unsigned index = 0;
    while (index < Dat_FileNames_count && !Dat_FileNames[index]) ++index;
    if (index == Dat_FileNames_count) bof3::Fatal("DIV-0089 self-test: Dat_FileNames is empty");
    const std::string bank = Dat_FileNames[index].Native();
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
    const bool missing = save_menu::g_stream_missing;  // DIV-0129's flag: the opens below fail
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
    save_menu::g_stream_missing = missing;
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
    const bool missing = save_menu::g_stream_missing;
    g.file_open = FailOpen;
    g.music_playing = StillPlaying;
    g.voice_playing = StillPlaying;
    save_menu::g_stream_missing = false;
    Sound_LoadStream(0);
    const int failed = Sound_StreamDone();
    const int original = g.music_playing() ^ 1;  // the control: the original's answer to the same state
    save_menu::g_stream_missing = false;                    // as after an open that succeeded
    const int present = Sound_StreamDone();
    g = saved;
    SaveMenu_StreamKind = kind;
    save_menu::g_stream_missing = missing;
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
}
