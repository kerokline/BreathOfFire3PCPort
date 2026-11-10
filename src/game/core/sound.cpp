// Moved here by tools/move_funcs.py from battle_sprites.cpp, display_env.cpp, save_menu.cpp, scena_sx2.cpp, shop_states2.cpp, sound.cpp, sound_rest.cpp (docs/source-layout-round.md).
#include "game/core/sound.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <io.h>
#include <malloc.h>
#include <vector>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_sprites_callees.h"
#include "game/cheats.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/display_env_callees.h"
#include "game/lang_layout.h"
#include "game/mode_flow.h"
#include "game/move_script_bytes.h"
#include "game/music_loops.h"
#include "game/music_resume.h"
#include "game/music_seq.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/save_menu.h"
#include "game/save_menu_callees.h"
#include "game/scena_sx2_callees.h"
#include "game/scenario_harness.h"
#include "game/shop_states2.h"
#include "game/shop_states2_callees.h"
#include "game/sound.h"
#include "game/sound_callees.h"
#include "game/sound_in.h"
#include "game/sound_rest.h"
#include "game/sound_rest_callees.h"
#include "game/sound_sp.h"
#include "game/sound_st.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "hook/snapshot.h"
#include "input/sigma.h"

namespace {

namespace at = scena_sx2::at;

}  // namespace

using namespace battle_sprites;

using namespace display_env;

using namespace save_menu;

using namespace shop_states2;

using namespace sound;

using namespace sound_rest;

namespace display_env {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Addr(const volatile void* p) { return Low32(p); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

// `rep movsd`: n dwords, first to last, each read just before it is stored -
// so a source that overlaps the destination from below repeats itself, as the
// original's does.
void CopyDwords(U dst, U src, U n) {
    for (U i = 0; i < n; ++i) PutLong(dst + i * 4, Long(src + i * 4));
}

}  // namespace

}  // namespace display_env

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

void PutFloat(save_menu::U address, float f) { std::memcpy(AtSaveMenu(address), &f, sizeof f); }

unsigned char* Packet() { return Gfx_PacketNext; }

// The state byte both menus run on, stepped as the originals step it.
save_menu::U State() { return SaveMenu_FlowState; }

void SetState(save_menu::U v) { SaveMenu_FlowState = v; }

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

// `call File_LoadDone; test eax, eax; jne done; loop: Task_Sleep(1);
// File_LoadDone; je loop` - three times in 0x5885D0.
void WaitLoad() {
    if (save_menu::g.load_done() != 0) return;
    do save_menu::g.task_sleep(1);
    while (save_menu::g.load_done() == 0);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

// The original's read: Save_ReadFile(name, 0x1C, 0xCA0), its result not
// tested, and Save_Staging's first 0x1C bytes copied into the summary.
bool StagedRead(const char* name, unsigned char* summary, void*) {
    save_menu::g.read_file(name, save_menu::at::kSummaryBytes, static_cast<int>(save_menu::at::kSummaryAt));
    for (save_menu::U i = 0; i < 7; ++i) PutLongSaveMenu(AddrSaveMenu(summary) + i * 4, LongSaveMenu(::bof3::Img(save_menu::at::kStaging.v) + i * 4));
    return true;
}

}  // namespace

#pragma clang attribute pop

namespace {

constexpr auto& AtScenaSx2 = move_script::At;

constexpr auto& LongScenaSx2 = move_script::Long;

using move_script::SetLong;

std::uint32_t UScenaSx2(std::int32_t v) { return static_cast<std::uint32_t>(v); }

std::uint32_t AddrScenaSx2(const void* p) { return Low32(p); }

unsigned char* AtShopStates2(shop_states2::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

shop_states2::U LongShopStates2(shop_states2::U address) {
    shop_states2::U v;
    std::memcpy(&v, AtShopStates2(address), sizeof v);
    return v;
}

using Handler = void (__cdecl*)();

// A dispatch's handler: entry `index` of a table in .data, read now. The
// index is not checked, as the originals have it: past its table the words
// after it are read - the next tables, then Shop_Equip's jump table and on.
// A null entry would be a call to 0 in the original; ours aborts.
Handler Entry(shop_states2::U table, shop_states2::U index, const char* who) {
    const shop_states2::U target = LongShopStates2(table + index * 4);
    if (target == 0) bof3::Fatal("%s: entry %u of the table 0x%X is null", who, static_cast<unsigned>(index), static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(target));
}

}  // namespace

namespace sound {

namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

std::uint32_t Dword(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void SetDword(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }

std::uint32_t Address(const volatile void* p) { return Low32(p); }

}  // namespace

}  // namespace sound

// The five with an eax result, under the assembler names of the void
// declarations symbols.gen.h makes (common/asm_name.h: the i686 ABI's leading
// underscore, none on x86-64).
extern "C" int __cdecl Sound_PlayEffectEax(unsigned short id) BOF3_ASM_NAME(Sound_PlayEffect);

extern "C" int __cdecl Music_PlayEax(unsigned int track, int frames) BOF3_ASM_NAME(Music_Play);

extern "C" int __cdecl Music_FadeOutStopEax(int frames) BOF3_ASM_NAME(Music_FadeOutStop);

extern "C" int __cdecl Music_FadeInEax(int frames) BOF3_ASM_NAME(Music_FadeIn);

extern "C" int __cdecl Music_FadeOutEax(int frames) BOF3_ASM_NAME(Music_FadeOut);

#define MUSIC_LOG(...)     do { if (sound::MusicLogOn()) bof3::Log("music_log   f%u " __VA_ARGS__); } while (0)

namespace sound {

namespace {

// Music_SetVolume with the float's bits, passed as the originals pass them
// (a dword move, never an x87 load that would quiet a signalling NaN): the
// bits copied into a float and handed over as the float parameter it is -
// an SSE move on both widths, so the bit pattern arrives whole. (Until the
// 64-bit build this called through a `void (*)(std::uint32_t)` type, which
// on i686 is the same stack dword and on x86-64 is rcx where the callee
// reads xmm0: the first 64-bit self-test read garbage volumes.)
void SetVolumeBits(std::uint32_t bits) {
    float volume;
    std::memcpy(&volume, &bits, sizeof volume);
    g.music_set_volume(volume);
}

}  // namespace

}  // namespace sound

namespace sound_rest {

namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

std::uint32_t Address(const volatile void* p) { return Low32(p); }

std::uint32_t Dword(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

}  // namespace

}  // namespace sound_rest

// Sound_PauseAll's eax: Music_Halt's, which its tail jump hands back. Defined
// under the assembler name of the void declaration symbols.gen.h makes, as
// sound.cpp's five are (common/asm_name.h); the one caller (Game_WndProc,
// ours) does not read it.
extern "C" int __cdecl Sound_PauseAllEax(void) BOF3_ASM_NAME(Sound_PauseAll);

// display_env's callee seam, moved here from display_env.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace display_env {

const Callees kOriginals = {
    Gfx_Present, Gfx_ClearPresent, D3d_SetBackColor, Crt_malloc, Crt_free, SndBuf_Release, SndBuf_FromWave,
};
Callees g = kOriginals;

}  // namespace display_env

// sound's callee seam, moved here from sound.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace sound {
namespace {

// Win32 through the game's own import slots, as the originals call it.
template <class Fn>
Fn Import(std::uint32_t slot) {
    std::uint32_t word;   // the import slot is a 32-bit word: Windows' function on i686, its thunk on x86-64 (design 4)
    std::memcpy(&word, At(slot), sizeof word);
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(word));
}
void* __stdcall ImportCreateEvent(void* security, int manual, int initial, const char* name) {
    return Import<void* (__stdcall*)(void*, int, int, const char*)>(::bof3::Img(Sound_ImportCreateEvent_addr))(security, manual, initial, name);
}
int __stdcall ImportCloseHandle(void* handle) { return Import<int (__stdcall*)(void*)>(::bof3::Img(Sound_ImportCloseHandle_addr))(handle); }
unsigned __stdcall ImportMsgWait(unsigned count, const void* handles, int all, unsigned ms, unsigned mask) {
    return Import<unsigned (__stdcall*)(unsigned, const void*, int, unsigned, unsigned)>(::bof3::Img(Sound_ImportMsgWait_addr))(count, handles, all,
                                                                                                         ms, mask);
}

}  // namespace

const Callees kOriginals = {
    Crt_sprintf, Crt_malloc, Crt_free, File_Open, File_Size, File_Read, File_Close,
    Mp3_Create, Mp3_MemoryIo, Mp3_SetIo, Mp3_Open, Mp3_Start, Mp3_Decode, Mp3_Seek, Mp3_Destroy,
    ImportCreateEvent, ImportCloseHandle, ImportMsgWait,
    Music_LoadFile, Music_SetVolume, Music_Start, Music_FadeInEax, Music_Pump, Music_Stop, Wave_FindData, SndBuf_Write,
    SndBuf_Play, SndBuf_Stop, SndBuf_CreateFromWave, Music_Release, Music_OpenDecoder, Music_CreateBuffer,
    Music_SetNotify, Music_Decode,
};
Callees g = kOriginals;

}  // namespace sound

// original 0x587740: a sound effect - the PSX SE_Play's cue of up to four
// chord voices, bank:id alike, on DirectSound buffers. The cue is the four
// dwords at Sound_Banks + (bank - 1) * 0x384 + cue * 16 with bank = id bits
// 8-11 and cue its low byte. A word of 0 is no voice. Otherwise its bits 8-12
// are a channel of Sound_Channels (23 or more less 23) and its low byte the
// bank's voice entry: 0xFF stops and forgets the channel's buffer; any other
// stops it, plays the voice's buffer at the word's high half as a frequency,
// looping when bit 15 is set, and remembers the buffer in the channel only
// then (0 otherwise), so a later cue can stop it.
// As the original has it: bank 0 reads the 0x384 bytes before the table and
// a cue past 23 reads on into the voice entries, a voice past 63 into the
// next bank; the entry's buffer is read again after SndBuf_Play; the word is
// read afresh for each voice. eax is 0 on return (the original's loop count).
int Sound_PlayEffectEax(unsigned short id) {
    const std::uint32_t cue_id = id & 0xFFFFu;
    const std::uint32_t base = (((cue_id >> 8) & 0xF) - 1) * sound::kBankBytes;
    std::uint32_t cue = sound::Address(Sound_Banks) + base + (cue_id & 0xFF) * 16;
    for (int n = 4; n != 0; --n, cue += 4) {
        const std::uint32_t word = sound::Dword(cue);
        if (word == 0) continue;
        std::uint32_t channel = (word >> 8) & 0x1F;
        if (channel >= 0x17) channel -= 0x17;
        const std::uint32_t voice = word & 0xFF;
        if (voice == 0xFF) {
            void* const playing = reinterpret_cast<void*>(Sound_Channels[channel]);
            if (playing) {
                sound::g.sndbuf_stop(playing);
                Sound_Channels[channel] = 0;
            }
            continue;
        }
        const std::uint32_t loops = (word & 0xFFFF8000u) << 16;
        void* const playing = reinterpret_cast<void*>(Sound_Channels[channel]);
        if (playing) sound::g.sndbuf_stop(playing);
        const std::uint32_t entry = sound::Address(Sound_Banks) + base + sound::kVoices + voice * 8;
        // DIVERGENCE DIV-0171 / DIV-0172: a buffer sound_sp changed gets its level back first (sound_sp.h).
        if (sound_sp::Routed()) sound_sp::BeforePlay(reinterpret_cast<void*>(sound::Dword(entry + 4)));
        sound::g.sndbuf_play(sound::Dword(entry) | loops, reinterpret_cast<void*>(sound::Dword(entry + 4)), word >> 16);
        Sound_Channels[channel] = loops ? sound::Dword(entry + 4) : 0;
        if (!loops) sound_st::NoteOneShot(channel, entry);  // DIV-0174: DLL-side, nothing without its switch
    }
    return 0;
}

// original 0x587860 (PSX 0x8015D8AC): each of the 23 Sound_Channels dwords (to 0x6BC924) not 0:
// SndBuf_Stop(it), then the dword 0 (after the call). Sound_PauseAll 0x587C30
// walks the same dwords but keeps them (and stops the stream and the music).
// The loss screen's BattleLoss_Restart 0x432750 (BE1's), chapter 15's
// Scena15_Run5 and chapter 17's Scena17_Outro11 / Scena17_EndRestart call it
// before Task_Restart(Boot_Task).
// PSX 0x8015D8AC (pairs_propagated "call").
extern "C" void __cdecl Sound_StopChannels(void) {
    for (std::uint32_t p = AddrScenaSx2(Sound_Channels); p < ::bof3::Img(scena_sx2::at::kChannelsEnd); p += 4) {
        const std::uint32_t buffer = UScenaSx2(LongScenaSx2(AtScenaSx2(p)));
        if (buffer == 0) continue;
        SH_CALL(SndBuf_Stop)(AtScenaSx2(buffer));
        SetLong(AtScenaSx2(p), 0);
    }
}

// original 0x587890: the volume of a cue's voices. The cue word (the first
// argument & 0xFFFF): bank ((cue >> 8) & 0xF) - 1 (bank 0 gives -1, a record
// before Sound_Banks: unchecked), its 16-byte cue (cue & 0xFF) at Sound_Banks
// + bank * 0x384 + cue * 16 - four dwords, each read again; for each not 0,
// 0x5A6C60(the buffer of voice (dword & 0xFF), the bank's +0x184 + voice * 8,
// the level dword whole) - a DirectSound SetVolume. As Sound_PlayEffect reads
// the same cue. Chapter 13's Scena13_ToneLevels is the one caller. No PSX
// twin paired.
extern "C" void __cdecl Sound_SetCueVolume(unsigned cue, unsigned level) {
    const std::uint32_t c = cue & 0xFFFF;
    const std::uint32_t bank = (((c >> 8) & 0xF) - 1) * scena_sx2::at::kBankStride;
    const std::uint32_t cue_at = AddrScenaSx2(Sound_Banks) + bank + (c & 0xFF) * 16;
    for (unsigned k = 0; k < 4; ++k) {
        const std::uint32_t voice = UScenaSx2(LongScenaSx2(AtScenaSx2(cue_at + 4 * k)));
        if (voice == 0) continue;
        const std::uint32_t buffer = UScenaSx2(LongScenaSx2(AtScenaSx2(AddrScenaSx2(Sound_Banks) + scena_sx2::at::kBankVoices + bank + (voice & 0xFF) * 8)));
        SH_AT(void (__cdecl*)(std::uint32_t, std::uint32_t), scena_sx2::at::kSndBufVolume.v)(buffer, level);
    }
}

// original 0x587900 (PSX 0x8015E210): Sound_PlayEffect(id). Ops BC, BD and D1 push more
// arguments; only the first is read.
//
// The PlayStation has three here (US 0x8015D71C / 0x8015D7C8 / 0x8015D820;
// JP 0x8015E10C / 0x8015E1B8 / 0x8015E210); the port folded them into one.
// Every caller but four is the first's (a jal scan of the US disc: 503 calls
// in 176 overlays; 0x8015D7C8 only from op BC and SCENA01's three, 0x8015D820
// only from ops BD and D1), and those four are ours and call sound_sp's
// level plays under DIV-0172 (move_groups.cpp, scena_sc1.cpp).
// DIVERGENCE DIV-0171: with the switch, the first's play: the cue follows
// Sprite_Current (sound_sp.h).
extern "C" void __cdecl Sound_PlayById(unsigned short id) {
    if (sound_sp::g_position) {
        sound_sp::PlayPositional(id);
        return;
    }
    battle_sprites::g.play_effect(id);
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

extern "C" void __cdecl Sound_LoadStream(unsigned id) {
    sound_in::StreamLoads();  // DIV-0177
    const save_menu::U kind = id >> 12;
    SaveMenu_StreamKind = kind;
    const save_menu::U table = LongSaveMenu(::bof3::Img(save_menu::at::kStreamTables.v) + kind * 4);
    const save_menu::U name = LongSaveMenu(table + (id & 0xFFF) * 4);
    char path[260];  // MAX_PATH
    // DIV-0150: a kind-0 stream from the chosen music source's order; else, and
    // for the other kinds, DIV-0089's cache lookup
    const char* const text = reinterpret_cast<const char*>(AtSaveMenu(name));
    const bool cache = kind == 0 && music_seq::StreamPath(text, path, sizeof path) ? path[0] != 0
                                                                                  : dat_cache::SoundPath(text, path, sizeof path);
    if (!cache) save_menu::g.sprintf(path, reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(save_menu::at::kStreamFormat.v))), text);
    const int handle = save_menu::g.file_open(path, 0, 0);
    if (handle == -1) {
        if (!save_menu::g_stream_missing)
            bof3::Log("sound       DIV-0129: %s is not there; a wait on it ends at once", path);
        save_menu::g_stream_missing = true;  // DIV-0129
        return;
    }
    save_menu::g_stream_missing = false;
    if (SaveMenu_StreamData != 0) save_menu::g.free(AtSaveMenu(SaveMenu_StreamData));
    const int size = save_menu::g.file_size(handle);
    void* const data = save_menu::g.malloc(static_cast<unsigned>(size));
    SaveMenu_StreamData = AddrSaveMenu(data);
    save_menu::g.file_read(handle, data, static_cast<unsigned>(size));
    save_menu::g.file_close(handle);
    if (SaveMenu_StreamKind != 0) {
        save_menu::g.voice_play(AtSaveMenu(SaveMenu_StreamData));
        bof3::RunSpeed_StreamStarted();
        return;
    }
    sound_in::JingleStarts();  // DIV-0177: the song heard, before the stop below and the jingle's start
    if (static_cast<std::uint32_t>(Music_FadeCount) != 0) {
        Music_FadeCount = 0;
        save_menu::g.music_stop();
        Music_Track = 0xFF;
    }
    save_menu::g.music_start(AtSaveMenu(SaveMenu_StreamData), static_cast<unsigned>(size), 0);
    save_menu::g.music_volume(save_menu::at::kStreamVolume);
    bof3::RunSpeed_StreamStarted();
}

// 0x587A00 (PSX 0x80164890 reads its stream's byte). 1 once the last stream
// has stopped: kind non-zero asks 0x5A7200, kind 0 Music_IsPlaying, and the
// answer's bit 0 is flipped (`xor eax, 1` on the whole register). Op 8B and
// the inn wait on it.
//
// DIVERGENCE DIV-0129: 1 while the last stream's file could not be opened.
// DIVERGENCE DIV-0177: 1 for a kind-0 jingle once its song has been started
// again - the music ring plays the song now, not the jingle.
extern "C" int __cdecl Sound_StreamDone(void) {
    if (save_menu::g_stream_missing) return 1;  // DIV-0129
    if (SaveMenu_StreamKind != 0) return save_menu::g.voice_playing() ^ 1;
    if (sound_in::JingleOver()) return 1;  // DIV-0177
    return save_menu::g.music_playing() ^ 1;
}

#pragma clang attribute pop

// original 0x587A20 (PSX 0x801625AC): reads BGM track `track` whole into Music_File. Both names
// are formatted first: the plain one (the track loops) and the N one (it
// plays once); Music_FileLoops is 1 before the plain one is tried and 0 only
// when the N one opened (docs/asset-loading-path.md section 1a).
// As the original has it: when neither opens it returns -1 with
// Music_FileLoops left 1 and nothing else changed - Music_Play then starts
// the previous file, looping (known-defects D24);
// the old file is freed only once a new one has opened; File_Read is asked
// for Music_FileSize as read back after the allocation, into the pointer the
// allocation returned. The original's success eax is File_Close's; ours is 0.
//
// DIVERGENCE DIV-0087: first, when armed (BOF3X_CACHE, BOF3X_MUSIC), the
// cache's base\bgm\NNN.DAT in the MP3's place (music_seq.h); not formatted,
// opened or read otherwise.
//
// DIVERGENCE DIV-0150: under BOF3X_MUSIC=pc the MP3 names first, and the
// cache's song only for a track the install lacks.
extern "C" int __cdecl Music_LoadFile(unsigned track) {
    const bool pc_first = music_seq::PcFirst();               // DIV-0150
    if (!pc_first && music_seq::LoadFile(track)) return 0;  // DIV-0087
    char looping[0x28], once[0x28];
    sound::g.sprintf(looping, reinterpret_cast<const char*>(::bof3::Img(kLoopingName.v)), track);
    sound::g.sprintf(once, reinterpret_cast<const char*>(::bof3::Img(kOnceName.v)), track);
    Music_FileLoops = 1;
    int handle = sound::g.file_open(looping, 0, 0);
    if (handle == -1) {
        handle = sound::g.file_open(once, 0, 0);
        if (handle == -1) return pc_first && music_seq::LoadFile(track) ? 0 : -1;  // DIV-0150
        Music_FileLoops = 0;
    }
    music_seq::Forget(Music_File);  // DIV-0087: a cache song in it is gone
    if (Music_File) sound::g.free(Music_File);
    const int size = sound::g.file_size(handle);
    Music_FileSize = size;
    void* const file = sound::g.malloc(static_cast<unsigned>(size));
    Music_File = file;
    sound::g.file_read(handle, file, static_cast<unsigned>(Music_FileSize));
    sound::g.file_close(handle);
    Music_LoadedTrack = static_cast<int>(track);
    return 0;
}

// original 0x587AE0 (PSX 0x80161B24 and 0x80161C20; JP 0x80162514 and
// 0x80162610): starts BGM track `track`, fading in over `frames`.
// Nothing if it is the track playing (Music_Track, the whole argument against
// the byte); else the byte is set, the file loaded unless Music_File already
// holds it, the volume set to 0 and the stream started from the top, and the
// fade begun.
// As the original has it: a track above 0xFF never equals the byte and is
// stored truncated; eax is the track when nothing was done, else the fade's
// (frames). Track 0xFF while a track plays is stored (nothing playing, as far
// as the game can see) and the stream restarted from the top of whatever
// Music_File still holds - Music_LoadFile finds no file 255 and leaves it
// (known-defects D346, D24).
//
// DIVERGENCE DIV-0168: with the switch, a selection whose low byte is 0xFF
// does nothing (eax the track), as both PlayStation twins do (`andi 0xFF;
// beq 0xFF` at JP 0x80162530 / 0x80162630, the US words the same). The test
// comes after DIV-0028's completion of a stopping fade, which only models what
// Capcom's own state would already be.
int Music_PlayEax(unsigned int track, int frames) {
    MUSIC_LOG("Music_Play(%u, %d) track %u loaded %d from %p", Frame_Counter, track, frames, (unsigned)Music_Track, Music_LoadedTrack, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    if (sound::g_play_refuses_none && (track & 0xFF) == 0xFF) return static_cast<int>(track);  // DIV-0168
    if (track == Music_Track) return static_cast<int>(track);
    const std::uint32_t loaded = static_cast<std::uint32_t>(Music_LoadedTrack);
    Music_Track = static_cast<unsigned char>(track);
    if (track != loaded) sound::g.music_load_file(track);
    sound::SetDword(sound::Address(&Music_Volume), 0);
    SetVolumeBits(0);
    sound::g.music_start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    return sound::g.music_fade_in(frames);
}

// original 0x587B40 (PSX 0x801626C0, US 0x80161CD0): fades the music to
// nothing over `frames` of Sound_Tick, then stops it. x87 as the original: the
// step is -(volume / frames) with frames loaded as a 64-bit integer whose high
// half is 0 - unsigned, where Music_FadeIn divides by it signed - rounded at
// the control word's precision, then stored as a float. eax is frames.
//
// DIVERGENCE DIV-0173: with the switch and frames above 0, the stream is then
// stopped at once (sound_st::CutStop), as the PlayStation's twin does: its
// decrescendo is followed by SsSepStop in the same call (0x8015DF44, US
// 0x8015D554). Every word above is still written as Capcom's, so the game sees
// the fade it always saw; only the sound ends now instead of `frames` later.
int Music_FadeOutStopEax(int frames) {
    MUSIC_LOG("Music_FadeOutStop(%d) track %u from %p", Frame_Counter, frames, (unsigned)Music_Track, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    const std::uint32_t wide[2] = {static_cast<std::uint32_t>(frames), 0};
    Music_FadeStops = 1;
    __asm__ volatile(
        "fildll %[n]\n\t"
        "fdivrs %[volume]\n\t"
        "fchs\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [n] "m"(wide), [volume] "m"(Music_Volume)
        : "st");
    Music_FadeCount = frames;
    sound::g_stop_unseen = sound::g_fade_per_frame && frames != 0;  // DIV-0028: the first tick marks it stopped
    if (sound_st::g_stop_cuts && frames > 0) sound_st::CutStop(frames);  // DIV-0173
    return frames;
}

// original 0x587B80: `jmp Music_Halt`. The caller's ecx and stack reach
// Music_Halt as they were: a jump through SoundRest_g, not a call. A pair by
// width (docs/x64-asm-policy.md 8 unit 5): the i686 operand's offset is the
// table of 4-byte pointers', the x86_64 half's is offsetof over the 8-byte
// ones, RIP-relative (sound_rest_callees.h's assert ties the two).
extern "C" __attribute__((naked)) void __cdecl Sound_StopMusic(void) {
#if defined(__i386__)
    __asm__ volatile("jmp *(" BOF3_SYM(SoundRest_g) "+8)");
#elif defined(__x86_64__)
    __asm__ volatile("jmp *%c0+" BOF3_SYM(SoundRest_g) "(%%rip)" : : "i"(offsetof(sound_rest::Callees, halt)));
#else
#error "no callee-table jump for this architecture"
#endif
}

// original 0x587B90: `jmp Music_Resume`, the same way.
extern "C" __attribute__((naked)) void __cdecl Sound_ResumeAll(void) {
#if defined(__i386__)
    __asm__ volatile("jmp *(" BOF3_SYM(SoundRest_g) "+12)");
#elif defined(__x86_64__)
    __asm__ volatile("jmp *%c0+" BOF3_SYM(SoundRest_g) "(%%rip)" : : "i"(offsetof(sound_rest::Callees, resume)));
#else
#error "no callee-table jump for this architecture"
#endif
}

// original 0x587BA0 (PSX 0x8015DE8C): fades the music up to 127 over `frames`: the step is
// (127.0 - volume) / frames with frames a signed dword (fidiv), x87 as the
// original. A fade in never stops the music. eax is frames.
int Music_FadeInEax(int frames) {
    MUSIC_LOG("Music_FadeIn(%d) track %u from %p", Frame_Counter, frames, (unsigned)Music_Track, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    __asm__ volatile(
        "flds %[full]\n\t"
        "fsubs %[volume]\n\t"
        "fidivl %[n]\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [full] "m"(*reinterpret_cast<const float*>(static_cast<std::uintptr_t>(k127At))), [volume] "m"(Music_Volume), [n] "m"(frames)
        : "st");
    Music_FadeStops = 0;
    Music_FadeCount = frames;
    return frames;
}

// original 0x587BE0: Music_FadeOutStop without the stop - the music plays on
// at nothing. eax is frames.
int Music_FadeOutEax(int frames) {
    MUSIC_LOG("Music_FadeOut(%d) track %u from %p", Frame_Counter, frames, (unsigned)Music_Track, __builtin_return_address(0));
    sound::FinishStoppingFade();  // DIV-0028
    const std::uint32_t wide[2] = {static_cast<std::uint32_t>(frames), 0};
    __asm__ volatile(
        "fildll %[n]\n\t"
        "fdivrs %[volume]\n\t"
        "fchs\n\t"
        "fstps %[step]\n\t"
        : [step] "=m"(Music_FadeStep)
        : [n] "m"(wide), [volume] "m"(Music_Volume)
        : "st");
    Music_FadeStops = 0;
    Music_FadeCount = frames;
    return frames;
}

// original 0x587C20: `jmp Music_IsPlaying`, the same way - Music_IsPlaying
// reads the ecx it is handed (save_menu.cpp). Its callers are Game_Mode 3's
// steps 0x495BC0 and 0x495C50, still Capcom's (docs/sound-rest.md section 5).
extern "C" __attribute__((naked)) int __cdecl Sound_MusicPlaying(void) {
#if defined(__i386__)
    __asm__ volatile("jmp *(" BOF3_SYM(SoundRest_g) "+16)");
#elif defined(__x86_64__)
    __asm__ volatile("jmp *%c0+" BOF3_SYM(SoundRest_g) "(%%rip)" : : "i"(offsetof(sound_rest::Callees, music_playing)));
#else
#error "no callee-table jump for this architecture"
#endif
}

// original 0x587C30: everything paused - WndProc's on a deactivation and on
// F9. Each of the 23 Sound_Channels dwords that is not 0, SndBuf_Stop (the
// dwords kept, unlike Sound_StopChannels'); with a stream kind above 0, the
// SND stream's buffer SndStream_Stop stops and releases; then the music,
// Music_Halt, whose eax is the answer.
// As the original has it: each dword read afresh, after the call before it;
// the kind read after the walk. The original's tail jump hands Music_Halt the
// ecx its last callee left (or its own caller's): ours hands it what ours
// leaves - a value no one set either way, read only where DirectSound's
// GetStatus does not write its out (section 3).
int Sound_PauseAllEax(void) {
    for (std::uint32_t p = sound_rest::Address(Sound_Channels); p < ::bof3::Img(sound_rest::kChannelsEnd); p += 4) {
        const std::uint32_t buffer = sound_rest::Dword(p);
        if (buffer != 0) SoundRest_g.sndbuf_stop(sound_rest::At(buffer));
    }
    if (SaveMenu_StreamKind != 0) SoundRest_g.stream_stop();
    return SoundRest_g.halt();
}

extern "C" void __cdecl Sound_Tick(void) {
    sound::g.music_pump();
    sound_in::JingleFrame();  // DIV-0177: once a logic frame, the jingle's end and its song's resume
    if (sound::MusicLogOn()) sound::RingLog();  // the music log's ear: no behaviour
    if (Music_FadeCount == 0) return;
    if (sound::g_fade_per_frame) {
        if (sound::g_stop_unseen) {
            sound::g_stop_unseen = 0;
            if (Music_FadeStops) Music_Track = 0xFF;  // stopped, as far as the game can see
        }
        const std::uint32_t frame = Frame_Counter;
        if (frame == sound::g_fade_frame) return;
        sound::g_fade_frame = frame;
    }
    __asm__ volatile(
        "flds %[step]\n\t"
        "fadds %[volume]\n\t"
        "fstps %[volume]\n\t"
        : [volume] "+m"(Music_Volume)
        : [step] "m"(Music_FadeStep)
        : "st");
    SetVolumeBits(sound::Dword(sound::Address(&Music_Volume)));
    const int count = Music_FadeCount - 1;
    Music_FadeCount = count;
    if (count != 0) return;
    if (!Music_FadeStops) return;
    sound::g.music_stop();
    Music_Track = 0xFF;
}

// 0x587CD0; callers LoadDatFile (0x454704, ours) and 0x4547E9, for each
// kind-2 chunk: bank = the chunk's tag. The record is Sound_Banks +
// (bank - 1) * 0x384, in 32 bits - nothing checks the bank (the shipped data
// has 1..6). If the record already holds sample data: Crt_free of it, then
// for each of the 64 voice entries whose buffer is set, SndBuf_Release of it
// and every Sound_Channels dword equal to the entry's buffer - read again
// after the release - cleared; the entries themselves are left. Then the
// payload's first 0x380 bytes over the record's head (a dword copy), and the
// rest - size - 0x380, unsigned, unchecked - to a Crt_malloc buffer that is
// not tested for null, dwords then bytes. Then each voice entry whose first
// dword is non-zero becomes data + offset - 0x380 (the data pointer read
// from the record each time) and its buffer SndBuf_FromWave of that. An
// entry whose offset is 0 keeps the buffer dword the payload's head carried.
void Snd_LoadBank(unsigned bank, const void* payload, unsigned size) {
    const display_env::U record = ::bof3::Img(Sound_Banks_addr) + (static_cast<display_env::U>(bank) - 1u) * display_env::kBankBytes;
    const display_env::U old = display_env::Long(record + kBankHeader);
    if (old != 0) {
        display_env::g.free(display_env::At(old));
        for (display_env::U i = 0; i < 64; ++i) {
            const display_env::U entry = record + display_env::kVoices + 4 + i * 8;
            const display_env::U buffer = display_env::Long(entry);
            if (buffer == 0) continue;
            sound_sp::Released(display_env::At(buffer));   // DIV-0171 / 0172's remembered voices (ours; nothing of Capcom's)
            sound_st::Released(buffer);       // DIV-0174's remembered one-shots
            display_env::g.release(display_env::At(buffer));
            const display_env::U now = display_env::Long(entry);
            for (display_env::U c = ::bof3::Img(Sound_Channels_addr); c < ::bof3::Img(display_env::kChannelsEnd); c += 4)
                if (display_env::Long(c) == now) PutLong(c, 0);
        }
    }
    const display_env::U src = Addr(payload);
    CopyDwords(record, src, kBankHeader / 4);
    const display_env::U bytes = static_cast<display_env::U>(size) - kBankHeader;
    const display_env::U data = Addr(display_env::g.malloc(bytes));
    PutLong(record + kBankHeader, data);
    CopyDwords(data, src + kBankHeader, bytes >> 2);
    for (display_env::U i = bytes & ~3u; i < bytes; ++i) display_env::At(data)[i] = display_env::At(src + kBankHeader)[i];
    for (display_env::U i = 0; i < 64; ++i) {
        const display_env::U entry = record + display_env::kVoices + i * 8;
        const display_env::U offset = display_env::Long(entry);
        if (offset == 0) continue;
        const display_env::U sample = display_env::Long(record + kBankHeader) + offset - kBankHeader;
        PutLong(entry, sample);
        PutLong(entry + 4, Addr(display_env::g.from_wave(display_env::At(sample))));
    }
}

// 0x587DB0, task 0's mode 1: `jmp [0x6671F4 + 4 Game_Step]`, the word
// unchecked. Steps 0 TitleFlow_Begin, 1 the title menu, 2 the new game, 3 the
// load menu, 4 the config screen ConfigScreen_Run 0x460CB0, 5 TitleFlow_EnterGame.
// A jump, as the original's: the step's function returns to task 0's loop.
extern "C" void __cdecl TitleFlow_Step(void) {
    using Step = void (__cdecl*)();
    const Step step = reinterpret_cast<Step>(AtSaveMenu(LongSaveMenu(::bof3::Img(save_menu::at::kStepTable.v) + static_cast<std::uint32_t>(Game_Step) * 4)));
    [[clang::musttail]] return step();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

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
    PutLongSaveMenu(::bof3::Img(save_menu::at::kFieldState.v), ::bof3::Img(ObjTrio_addr));
    PutLongSaveMenu(::bof3::Img(save_menu::at::kSpriteCurrent.v), ::bof3::Img(ObjTrio_addr));
    save_menu::g.member_sprite(0, 0);
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
    save_menu::g.set_animation(static_cast<unsigned char>(static_cast<save_menu::U>(rv::Current<records::SpriteObject>()->type) + 8));
    AreaW0b_PartySet = 0xFF;
    const save_menu::U any = save_menu::g.list_files() != 0 ? 1 : 0;
    Game_Step = static_cast<std::uint32_t>(Game_Step) + 1;
    SaveMenu_Flow9039D6 = 0;
    SetState(0);
    SaveMenu_AnySave = any;
    SaveMenu_Cursor = any;
}

#pragma clang attribute pop

// 0x587EA0, Game_Step 1: `jmp [0x66720C + 4 state]` - the title menu's
// states 0..6 (the table runs on into the load menu's).
extern "C" void __cdecl TitleFlow_Menu(void) {
    using Step = void (__cdecl*)();
    const Step step = reinterpret_cast<Step>(AtSaveMenu(LongSaveMenu(::bof3::Img(save_menu::at::kMenuTable.v) + State() * 4)));
    [[clang::musttail]] return step();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// 0x587EB0, title menu state 0: the frame, counter 2, state + 1.
extern "C" void __cdecl TitleMenu_Open(void) {
    save_menu::g.draw_frame();
    PutByte(::bof3::Img(save_menu::at::kCounter.v), 2);
    SetState(State() + 1);
}

// 0x587ED0, state 1: the frame; state + 1 when the counter, decremented,
// reaches 0.
extern "C" void __cdecl TitleMenu_OpenWait(void) {
    save_menu::g.draw_frame();
    const save_menu::U c = (Byte(::bof3::Img(save_menu::at::kCounter.v)) - 1) & 0xFF;
    PutByte(::bof3::Img(save_menu::at::kCounter.v), c);
    if (c == 0) SetState(State() + 1);
}

// 0x587EF0, state 2: the frame, bit 1 of 0x7DEE44, state + 1.
extern "C" void __cdecl TitleMenu_FrameUp(void) {
    save_menu::g.draw_frame();
    PutByte(::bof3::Img(save_menu::at::kMenuFlags.v), Byte(::bof3::Img(save_menu::at::kMenuFlags.v)) | 2);
    SetState(State() + 1);
}

// 0x587F10, state 3: the counter and all three row brightnesses (and the
// byte after them - a dword store) 0; the frame, the rows with glow, state + 1.
extern "C" void __cdecl TitleMenu_RowsStart(void) {
    PutByte(::bof3::Img(save_menu::at::kCounter.v), 0);
    PutLongSaveMenu(::bof3::Img(save_menu::at::kRowBright.v), 0);
    save_menu::g.draw_frame();
    save_menu::g.draw_rows(1);
    SetState(State() + 1);
}

// 0x587F40, state 4: the frame, the rows with glow; the counter + 0x10;
// every row brightness + 8, held at 0x40 (unsigned) except the cursor's row,
// which rises unheld (it wraps past 0xFF); state + 1 when the counter comes
// round to 0 - sixteen frames.
extern "C" void __cdecl TitleMenu_RowsFadeIn(void) {
    save_menu::g.draw_frame();
    save_menu::g.draw_rows(1);
    PutByte(::bof3::Img(save_menu::at::kCounter.v), Byte(::bof3::Img(save_menu::at::kCounter.v)) + 0x10);
    const int cursor = SaveMenu_Cursor;
    for (int i = 0; i < 3; ++i) {
        const save_menu::U b = (Byte(::bof3::Img(save_menu::at::kRowBright.v) + i) + 8) & 0xFF;
        PutByte(::bof3::Img(save_menu::at::kRowBright.v) + i, cursor != i && b > 0x40 ? 0x40 : b);
    }
    if (Byte(::bof3::Img(save_menu::at::kCounter.v)) == 0) SetState(State() + 1);
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
    save_menu::g.draw_frame();
    save_menu::g.draw_rows(0);
    if ((Byte(::bof3::Img(save_menu::at::kMenuFlags.v)) & 2) == 0) return;
    const save_menu::U pressed = LongSaveMenu(::bof3::Img(save_menu::at::kPressed.v));
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
        const save_menu::U a = ::bof3::Img(save_menu::at::kRowBright.v) + static_cast<save_menu::U>(i);
        if (cursor == i) {
            PutByte(a, Byte(a) + 8);
            if (Byte(a) > 0x80) PutByte(a, 0x80);
        } else {
            PutByte(a, Byte(a) + 0xF8);
            if (Byte(a) < 0x40) PutByte(a, 0x40);
        }
    }
    if ((static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) == 0 && (pressed & 0x800) == 0) return;
    save_menu::g.transition(0);
    SetState(State() + 1);
    save_menu::g.play_effect(0x105);
}

// 0x5880A0, state 6: once no transition runs (the wait word 0), Game_Step =
// cursor + 2 - the new game, the load, or the config screen - and state 0;
// until then the frame and the rows.
extern "C" void __cdecl TitleMenu_Leave(void) {
    if (Transition_Busy == 0) {
        const save_menu::U step = static_cast<save_menu::U>(SaveMenu_Cursor + 2);
        SetState(0);
        Game_Step = step;
        return;
    }
    save_menu::g.draw_frame();
    save_menu::g.draw_rows(0);
}

// 0x5880E0, Game_Step 2, the new game. NewGame_InitCharacters; 0x9039F2
// cleared; Game_AreaNumber 0xFFFF; Scenario_Start(0); the first dword of
// every 8 bytes of Cond_Flags up to 0x904030 cleared - half of the 0xA0
// bytes, as the original's stride has it; Transition_Start(0); Game_Step 5.
extern "C" void __cdecl TitleFlow_NewGame(void) {
    save_menu::g.init_characters();
    SaveMenu_NewGameByte = 0;
    Game_AreaNumber = 0xFFFF;
    save_menu::g.scenario_start(0);
    for (save_menu::U a = ::bof3::Img(Cond_Flags_addr); a < ::bof3::Img(save_menu::at::kCondFlagsEnd.v); a += 8) PutLongSaveMenu(a, 0);
    save_menu::g.transition(0);
    Game_Step = 5;
}

#pragma clang attribute pop

// 0x588130, Game_Step 3: `jmp [0x667228 + 4 state]` - the load menu's states
// 0..7.
extern "C" void __cdecl TitleFlow_Load(void) {
    using Step = void (__cdecl*)();
    const Step step = reinterpret_cast<Step>(AtSaveMenu(LongSaveMenu(::bof3::Img(save_menu::at::kLoadTable.v) + State() * 4)));
    [[clang::musttail]] return step();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// 0x588140, load menu state 0: Transition_Start(1); the cursor slot and the
// first shown 0; Save_ListFiles - with files Save_ReadSummaries and state
// + 1, without Sound_PlayEffect(0x107), error 1 and state 5.
extern "C" void __cdecl LoadMenu_List(void) {
    save_menu::g.transition(1);
    Rest2c_Slot = 0;
    PutLongSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v), 0);
    if (save_menu::g.list_files() != 0) {
        save_menu::g.read_summaries();
        SetState(State() + 1);
        return;
    }
    save_menu::g.play_effect(0x107);
    SaveMenu_ErrorKind = 1;
    SetState(5);
}

// 0x588190, state 1: the windows, message 0xAC, the slots with the cursor
// lit; state + 1 once no transition runs.
extern "C" void __cdecl LoadMenu_Open(void) {
    LoadWindows();
    LoadMessage(0xAC);
    save_menu::g.draw_slots(0x20, 0x30, 1);
    if (Transition_Busy == 0) SetState(State() + 1);
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
    save_menu::g.draw_slots(0x20, 0x30, 1);
    const save_menu::U repeat = save_menu::g.auto_repeat(static_cast<std::uint32_t>(Input_Pressed) & 0x5000);
    save_menu::U slot = LongSaveMenu(::bof3::Img(save_menu::at::kSlot.v));
    const save_menu::U old = slot & 0xFF;
    if ((repeat & 0x1000) && slot != 0) {
        --slot;
        Rest2c_Slot = slot;
        const save_menu::U top = LongSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v));
        if (static_cast<int>(slot) < static_cast<int>(top)) PutLongSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v), top - 1);
    } else if ((repeat & 0x4000) && static_cast<int>(slot) < 15) {
        ++slot;
        Rest2c_Slot = slot;
        if (static_cast<int>(slot) > static_cast<int>(LongSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v)) + 2)) PutLongSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v), LongSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v)) + 1);
    }
    if (old != slot) {
        save_menu::g.play_effect(0x101);
        slot = Rest2c_Slot;
    }
    const save_menu::U pressed = Input_Pressed;
    if (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed) {
        if (Byte(::bof3::Img(save_menu::at::kSummaries.v) + slot * save_menu::at::kSummaryBytes + 0x15) != 0xFF) {
            save_menu::g.play_effect(0x104);
            const save_menu::U s = State();
            FieldS_Answer = 1;
            SetState(s + 1);
            return;
        }
        save_menu::g.play_effect(0x107);
        return;
    }
    if (static_cast<std::uint32_t>(Field_CancelButtons) & pressed) {
        save_menu::g.play_effect(0x106);
        SetState(0);
        Game_Step = 1;
    }
}

// 0x588340, state 3: the windows, message 0xB9 (load this game?), the slots
// unlit; Menu_YesNo - on an answer, state + 1 for yes (0x929F0B), - 1 for no.
extern "C" void __cdecl LoadMenu_Confirm(void) {
    LoadWindows();
    LoadMessage(0xB9);
    save_menu::g.draw_slots(0x20, 0x30, 0);
    if (save_menu::g.yes_no() == 0) return;
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
    save_menu::g.draw_slots(0x20, 0x30, 0);
    const save_menu::U slot = Rest2c_Slot;
    for (save_menu::U i = 0; i < save_menu::at::kClearBytes; i += 4) PutLongSaveMenu(::bof3::Img(save_menu::at::kStaging.v) + i, 0);
    save_menu::g.sprintf(reinterpret_cast<char*>(AtSaveMenu(::bof3::Img(save_menu::at::kPath.v))), reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(save_menu::at::kSaveName.v))), slot);
    if (save_menu::g.read_file(reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(save_menu::at::kPath.v))), save_menu::at::kFileBytes, 0) == -1) {
        SaveMenu_ErrorKind = 2;
        SetState(5);
        return;
    }
    const save_menu::U stored = LongSaveMenu(::bof3::Img(save_menu::at::kChecksum.v));
    PutWord(::bof3::Img(save_menu::at::kChecksum.v), 0);
    save_menu::U sum = 0;
    for (save_menu::U i = 0; i < save_menu::at::kBlockBytes; ++i) {
        const save_menu::U c = Byte(::bof3::Img(save_menu::at::kStaging.v) + i);
        PutByte(::bof3::Img(save_menu::at::kGameBlock.v) + i, c);
        sum += c;
    }
    Field_ScriptFlags = static_cast<std::uint32_t>(SaveMenu_ScriptFlagsSaved);
    if ((sum & 0xFFFF) == (stored & 0xFFFF)) {
        // Instrumentation, not behaviour: tools/input_run.py hands slot 0 back
        // on this line (docs/input-script.md section 1a) - the file is not
        // read again until a save menu lists the slots.
        bof3::Log("save        loaded slot %X (%s)", (unsigned)slot, reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(save_menu::at::kPath.v))));
        SetState(6);
        return;
    }
    save_menu::g.play_effect(0x107);
    SaveMenu_ErrorKind = 2;
    SetState(5);
}

#pragma clang attribute pop

extern "C" __attribute__((naked)) void __cdecl LoadMenu_Error(void) {
#if defined(__i386__)
    __asm__ volatile(
        "push %ecx\n\t"
        "call " BOF3_SYM(SaveMenu_LoadErrorBody) "\n\t"
        "add $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    __asm__ volatile("jmp " BOF3_SYM(SaveMenu_LoadErrorBody));   // the caller's ecx is the body's first argument already
#else
#error "no ecx entry for this architecture"
#endif
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// 0x588560, state 6, loaded: the windows, the slots, message 0xB1;
// Transition_Start(0); state + 1 (read after it); 30 frames before the game
// is applied.
extern "C" void __cdecl LoadMenu_Loaded(void) {
    LoadWindows();
    save_menu::g.draw_slots(0x20, 0x30, 0);
    LoadMessage(0xB1);
    save_menu::g.transition(0);
    const save_menu::U s = State();
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
    const save_menu::U delay = SaveMenu_ApplyDelay;
    SaveMenu_ApplyDelay = delay - 1;
    if (delay == 0) {
        const save_menu::U map_4 = LongSaveMenu(::bof3::Img(save_menu::at::kSavedButtons.v) + 4), map_0 = SaveMenu_SavedButtons, map_8 = LongSaveMenu(::bof3::Img(save_menu::at::kSavedButtons.v) + 8);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kButtons.v) + 4, map_4);
        const save_menu::U map_10 = WordSaveMenu(::bof3::Img(save_menu::at::kSavedButtons.v) + 0x10);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kButtons.v), map_0);
        const save_menu::U map_c = LongSaveMenu(::bof3::Img(save_menu::at::kSavedButtons.v) + 0xC);
        PutWord(::bof3::Img(save_menu::at::kButtons.v) + 0x10, map_10);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kButtons.v) + 8, map_8);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kButtons.v) + 0xC, map_c);
        const bool reset = SaveMenu_DefaultPad != 0;
        if (reset) {
            PutWord(::bof3::Img(save_menu::at::kButtons.v), 0x20);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 2, 0x80);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 0xC, 0x40);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 4, 0x10);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 6, 8);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 8, 4);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 0xE, 0x20);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 0x10, 0x40);
            PutWord(::bof3::Img(save_menu::at::kButtons.v) + 0xA, 0x100);
        }
        // DIV-0133 and DIV-0134 (docs/button-sigma.md): the Start-held reset
        // by source, then words 7 / 8 as Speak's and Action's and the table
        // (a save whose six lines are not one button each refused) - nothing
        // until sigma::Arm, after every module's self-test.
        bof3x::input::sigma::LoadApplied(reset);
        Field_MemberCount = static_cast<save_menu::U>(save_menu::g.party_count(0));
        save_menu::g.party_select(static_cast<std::uint32_t>(AreaW0b_PartySet) & 0x7F, 3);
        WaitLoad();
        save_menu::g.party_select(static_cast<std::uint32_t>(AreaW0b_PartySet) & 0x7F, 0);
        WaitLoad();
        save_menu::g.party_load(0);
        const save_menu::U d8 = LongSaveMenu(::bof3::Img(save_menu::at::kGameBlock.v) + 8), d0 = LongSaveMenu(::bof3::Img(save_menu::at::kGameBlock.v)), d4 = LongSaveMenu(::bof3::Img(save_menu::at::kGameBlock.v) + 4);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kPosition.v) + 8, d8);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kPosition.v) + 4, d4);
        const save_menu::U dc = LongSaveMenu(::bof3::Img(save_menu::at::kGameBlock.v) + 0xC);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kPosition.v), d0);
        PutLongSaveMenu(::bof3::Img(save_menu::at::kPosition.v) + 0xC, dc);
        const save_menu::U c0 = Byte(::bof3::Img(save_menu::at::kGameBlock.v) + 0x20);
        const save_menu::U cond = ::bof3::Img(Cond_Flags_addr) + static_cast<save_menu::U>(static_cast<int>(static_cast<signed char>(d0 & 0xFF)) * 8);
        const save_menu::U c1 = Byte(::bof3::Img(save_menu::at::kGameBlock.v) + 0x21);
        Chapter_FlagRow = cond;
        const save_menu::U c2 = Byte(::bof3::Img(save_menu::at::kGameBlock.v) + 0x22);
        PutByte(::bof3::Img(save_menu::at::kCounters.v), c0);
        const save_menu::U c3 = Byte(::bof3::Img(save_menu::at::kGameBlock.v) + 0x23);
        PutByte(::bof3::Img(save_menu::at::kCounters.v) + 1, c1);
        PutByte(::bof3::Img(save_menu::at::kCounters.v) + 2, c2);
        PutByte(::bof3::Img(save_menu::at::kCounters.v) + 3, c3);
        save_menu::g.scenario_load();
        WaitLoad();
        Game_Step = 5;
        SetState(0);
    }
    if (Transition_Busy == 0) return;
    if (LongSaveMenu(::bof3::Img(save_menu::at::kHeld.v)) & 0x800) SaveMenu_DefaultPad = 1;
    LoadWindows();
    save_menu::g.draw_slots(0x20, 0x30, 0);
    LoadMessage(0xB1);
}

// 0x588800, Game_Step 5 (PSX 0x801E6228, 'callers'): nothing while a
// transition runs. Then 0x929F02, 0x929F03, 0x929F05 cleared; the cursor on
// row 2 (config) goes back to the title menu - state 0, Game_Step 1. Else
// for each of the eight characters NewGame_RollGrowth and Char_RecalcStats
// of its record - after a load too - and task 0 restarted at 0x495800, the
// game's body (0x5A9976, which does not return).
extern "C" void __cdecl TitleFlow_EnterGame(void) {
    if (Transition_Busy != 0) return;
    const save_menu::U cursor = static_cast<unsigned char>(SaveMenu_Cursor);
    FieldS_Step = 0;
    PutByte(::bof3::Img(save_menu::at::kMenuStates.v) + 1, 0);
    PutByte(::bof3::Img(save_menu::at::kMenuStates.v) + 3, 0);
    if (cursor == 2) {
        SetState(0);
        Game_Step = 1;
        return;
    }
    for (save_menu::U i = 0; i < 8; ++i) {
        save_menu::g.roll_growth(i);
        save_menu::g.recalc(AtSaveMenu(::bof3::Img(CharacterRecords_addr) + i * save_menu::at::kCharStride));
    }
    save_menu::g.task_restart(::bof3::Code(save_menu::at::kGameTask.v));
}

// 0x588880 (docs/title-menu.md section 2). With something to load, rows 0, 1,
// 2 at y 0x50, 0x70, 0x90; otherwise rows 0 and 2 at 0x60, 0x80. `glow` is
// passed through whole.
extern "C" void __cdecl TitleMenu_DrawRows(unsigned glow) {
    if (SaveMenu_AnySave != 0) {
        save_menu::g.draw_row(0x50, 0, glow);
        save_menu::g.draw_row(0x70, 1, glow);
        save_menu::g.draw_row(0x90, 2, glow);
        return;
    }
    save_menu::g.draw_row(0x60, 0, glow);
    save_menu::g.draw_row(0x80, 2, glow);
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
    const save_menu::U r = row & 0xFF;
    const save_menu::U width = TitleMenu_Width(r < 3 ? r : 0);   // at::kRowWidths[r], as title_menu.cpp holds it
    // A row past 2 reads the stack beyond the three bytes in the original:
    // nothing calls it so, and ours refuses rather than invent a width.
    if (r >= 3) bof3::Fatal("TitleMenu_DrawRow: row %u, the original reads past its width table", (unsigned)r);
    const float fx = static_cast<float>(static_cast<int>(0xA0 - (width >> 1)));
    const float fy = static_cast<float>(static_cast<short>(y));
    if ((glow & 0xFF) != 0) {
        const save_menu::U glow_page = save_menu::g.get_tpage(0, 2, 0x380, 0) & 0xFFFF;   // before Gfx_PacketNext is read
        save_menu::g.set_draw_mode(Packet(), 0, 0, glow_page, 0);
        save_menu::g.commit(1, 0xC);
        unsigned char* const p = Packet();
        auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
        save_menu::g.set_sprt(p);
        s->u = 0;
        s->h = 0x20;
        s->x = fx;
        s->y = fy;
        rv::Store<save_menu::U>(s->v, r << 5);
        rv::Store<save_menu::U>(s->w, width);
        rv::Store<save_menu::U>(s->clut, save_menu::g.get_clut(0, 0x1EB));
        rv::Store<save_menu::U>(s->r, Byte(::bof3::Img(save_menu::at::kRowBright.v) + r) << 1);
        rv::Store<save_menu::U>(s->g, Byte(::bof3::Img(save_menu::at::kRowBright.v) + r) << 1);
        rv::Store<save_menu::U>(s->b, Byte(::bof3::Img(save_menu::at::kRowBright.v) + r) << 1);
        save_menu::g.set_semi(p, glow & 0xFF);
        save_menu::g.commit(1, 0x1C);
    }
    const save_menu::U page = save_menu::g.get_tpage(0, 1, 0x380, 0) & 0xFFFF;
    save_menu::g.set_draw_mode(Packet(), 0, 0, page, 0);
    save_menu::g.commit(1, 0xC);
    unsigned char* const p = Packet();
    auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
    save_menu::g.set_sprt(p);
    s->u = 0;
    rv::Store<save_menu::U>(s->v, r << 5);
    s->h = 0x20;
    s->x = fx;
    s->y = fy;
    rv::Store<save_menu::U>(s->w, width);
    rv::Store<save_menu::U>(s->clut, save_menu::g.get_clut(0x20, 0x1E3));
    rv::Store<save_menu::U>(s->r, Byte(::bof3::Img(save_menu::at::kRowBright.v) + r));
    rv::Store<save_menu::U>(s->g, Byte(::bof3::Img(save_menu::at::kRowBright.v) + r));
    rv::Store<save_menu::U>(s->b, Byte(::bof3::Img(save_menu::at::kRowBright.v) + r));
    save_menu::g.set_semi(p, glow & 0xFF);
    save_menu::g.commit(1, 0x1C);
}

// 0x588AC0 (PSX 0x801E7190). For character (member & 0xFF), from its three
// bytes {lo, hi, n} at 0x667248: five NewGame_RollSum, the first four
// results mapped through 0x667260 into 0x903640 + 5 member + 0..3; then all
// five bytes cleared. Nothing survives but the Rand calls; the PSX twin
// does exactly the same, the fifth result stored and cleared too.
extern "C" void __cdecl NewGame_RollGrowth(unsigned member) {
    const save_menu::U m = member & 0xFF;
    const save_menu::U lo = Byte(::bof3::Img(save_menu::at::kGrowthTable.v) + m * 3), hi = Byte(::bof3::Img(save_menu::at::kGrowthTable.v) + m * 3 + 1),
            n = Byte(::bof3::Img(save_menu::at::kGrowthTable.v) + m * 3 + 2);
    const save_menu::U out = ::bof3::Img(save_menu::at::kGrowth.v) + m * 5;
    for (save_menu::U k = 0; k < 4; ++k) PutByte(out + k, Byte(::bof3::Img(save_menu::at::kGrowthValues.v) + save_menu::g.roll_sum(lo, hi, n)));
    save_menu::g.roll_sum(lo, hi, n);
    for (save_menu::U k = 0; k < 5; ++k) PutByte(out + k, 0);
}

// 0x588BA0 (PSX 0x801E70DC). The sum, as a byte, of n (a byte) values
// Rand() % (hi - lo + 1) + lo - the remainder of a signed division by the
// difference of the two bytes plus one, its low byte added to lo's. No check:
// hi = lo - 1 divides by zero as the original does.
extern "C" unsigned char __cdecl NewGame_RollSum(unsigned lo, unsigned hi, unsigned n) {
    unsigned char sum = 0;
    const int span = static_cast<int>(hi & 0xFF) - static_cast<int>(lo & 0xFF) + 1;
    for (save_menu::U k = n & 0xFF; k != 0; --k) {
        const int r = save_menu::g.rand() % span;
        sum = static_cast<unsigned char>(sum + static_cast<unsigned char>(r + static_cast<int>(lo & 0xFF)));
    }
    return sum;
}

// 0x588C00. The title menu's frame: draw mode page 0xBB, the pieces 0 at
// (0x1A, 0x18) and 1 at (0x10A, 0x88); draw mode page 0xB9, the pieces 2 at
// (-6, 0x1C) and 3 at (0xDA, 0x1C); every one on slot 2.
extern "C" void __cdecl TitleMenu_DrawFrame(void) {
    save_menu::g.set_draw_mode(Packet(), 0, 0, 0xBB, 0);
    save_menu::g.commit(2, 0xC);
    save_menu::g.draw_piece(0x1A, 0x18, 0, 2);
    save_menu::g.draw_piece(0x10A, 0x88, 1, 2);
    save_menu::g.set_draw_mode(Packet(), 0, 0, 0xB9, 0);
    save_menu::g.commit(2, 0xC);
    save_menu::g.draw_piece(-6, 0x1C, 2, 2);
    save_menu::g.draw_piece(0xDA, 0x1C, 3, 2);
}

// 0x588C90. One SPRT at Gfx_PacketNext: colour 0x20 on all three channels,
// (s16 x, s16 y) as floats, u / v / w / h / CLUT from record (index & 0xFF)
// of the 10-byte table 0x66726C (u and v as bytes, the CLUT word shifted left
// 6), then Gfx_CommitPrim(slot, 0x1C) - slot the whole dword.
extern "C" void __cdecl TitleMenu_DrawPiece(int x, int y, unsigned index, unsigned slot) {
    unsigned char* const p = Packet();
    auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
    const save_menu::U q = AddrSaveMenu(p);
    save_menu::g.set_sprt(p);
    s->r = 0x20;
    s->g = 0x20;
    s->b = 0x20;
    // The two floats by address: typed, the compiler pairs them into one
    // vector store (codegen_same).
    PutFloat(q + 8, static_cast<float>(static_cast<short>(x)));
    PutFloat(q + 0xC, static_cast<float>(static_cast<short>(y)));
    const save_menu::U e = ::bof3::Img(save_menu::at::kPieces.v) + (index & 0xFF) * 10;
    rv::Store<save_menu::U>(s->u, Byte(e));
    rv::Store<save_menu::U>(s->v, Byte(e + 2));
    rv::Store<save_menu::U>(s->w, WordSaveMenu(e + 4));
    rv::Store<save_menu::U>(s->h, WordSaveMenu(e + 6));
    rv::Store<save_menu::U>(s->clut, WordSaveMenu(e + 8) << 6);
    save_menu::g.commit(slot, 0x1C);
}

extern "C" void __cdecl SaveMenu_DrawSlots(int x, int y, unsigned highlight) {
    save_menu::DrawSlotsOver(x, y, highlight, AtSaveMenu(::bof3::Img(save_menu::at::kSummaries.v)), reinterpret_cast<const volatile save_menu::U*>(AtSaveMenu(::bof3::Img(save_menu::at::kSlotTop.v))),
                             reinterpret_cast<const volatile save_menu::U*>(AtSaveMenu(::bof3::Img(Rest2c_Slot_addr))));
}

// 0x588DC0 (docs/save-files.md): the body above on Save_Directory and the
// summaries at 0x905BC0, through Save_ReadFile and Save_Staging. Kept: the
// read's answer is not tested, so a file that will not open leaves the
// summary whatever Save_Staging held. It rebuilds from Save_Directory alone:
// DIV-0002's re-listing in Save_WriteFile is what keeps a new save in it.
extern "C" void __cdecl Save_ReadSummaries(void) {
    save_menu::ReadSummaries(AtSaveMenu(::bof3::Img(Save_Directory_addr)), AtSaveMenu(::bof3::Img(save_menu::at::kSummaries.v)), &StagedRead, nullptr);
}

// original 0x588E70: task 0's entry for the title (Title_StateMenu creates
// it). Game_Mode and Game_Step 0; Task_ClearPrivate; Window_ResetAll; then
// for ever: Task_Sleep(1), the entry of TitleTask_Modes 0x667294 that
// Game_Mode (a word, read afresh) picks - 0 TitleMode_Load, 1 TitleFlow_Step
// - and Field_RunTaskRecords. The task leaves only through Task_Restart
// (TitleFlow_EnterGame). As the original has it, the mode is not checked:
// the table is two long and a 0 follows (ours aborts on a null entry).
extern "C" void __cdecl TitleTask_Run(void) {
    Game_Mode = 0;
    Game_Step = 0;
    shop_states2::g.task_clear();
    shop_states2::g.window_reset();
    for (;;) {
        shop_states2::g.task_sleep(1);
        Entry(::bof3::Img(TitleTask_Modes_addr), LongShopStates2(::bof3::Img(shop_states2::at::kGameMode.v)) & 0xFFFF, "TitleTask_Run")();
        shop_states2::g.run_task_records();
    }
}

// original 0x588EB0: task 0's mode 0 - LoadDatFile(0x31D), Task_Sleep(1)
// until File_LoadDone, Gfx_ClutStripRestore, then Game_Mode + 1 and the CLUT
// strip's dirty byte + 1 (read before).
extern "C" void __cdecl TitleMode_Load(void) {
    shop_states2::g.load_dat(static_cast<int>(shop_states2::at::kTitleFile));
    if (shop_states2::g.load_done() == 0) {
        do shop_states2::g.task_sleep(1);
        while (shop_states2::g.load_done() == 0);
    }
    shop_states2::g.clut_restore();
    const shop_states2::U dirty = Gfx_ClutStripDirty;
    Game_Mode = static_cast<std::uint32_t>(Game_Mode) + 1;
    Gfx_ClutStripDirty = dirty + 1;
}

#pragma clang attribute pop

void CoreSound_Inject() {
    BOF3_INJECT(Sound_PlayEffect);
    BOF3_INJECT(Sound_StopChannels);
    BOF3_INJECT(Sound_SetCueVolume);
    BOF3_INJECT(Sound_PlayById);
    BOF3_INJECT(Sound_LoadStream);
    BOF3_INJECT(Sound_StreamDone);
    BOF3_INJECT(Music_LoadFile);
    BOF3_INJECT(Music_Play);
    BOF3_INJECT(Music_FadeOutStop);
    BOF3_INJECT(Sound_StopMusic);
    BOF3_INJECT(Sound_ResumeAll);
    BOF3_INJECT(Music_FadeIn);
    BOF3_INJECT(Music_FadeOut);
    BOF3_INJECT(Sound_MusicPlaying);
    BOF3_INJECT(Sound_PauseAll);
    BOF3_INJECT(Sound_Tick);
    BOF3_INJECT(Snd_LoadBank);
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
    BOF3_INJECT(TitleMenu_DrawRows);
    BOF3_INJECT(TitleMenu_DrawRow);
    BOF3_INJECT(NewGame_RollGrowth);
    BOF3_INJECT(NewGame_RollSum);
    BOF3_INJECT(TitleMenu_DrawFrame);
    BOF3_INJECT(TitleMenu_DrawPiece);
    BOF3_INJECT(SaveMenu_DrawSlots);
    BOF3_INJECT(Save_ReadSummaries);
    BOF3_INJECT(TitleTask_Run);
    BOF3_INJECT(TitleMode_Load);
}
