// The PlayStation's positional sound on the PC's flat effects - group SP of
// the sound-layer round (docs/sound-layer-round.md; known-defects D345).
// Three hooks the port emptied, each restored under a switch of its own:
//
//   DIV-0170  op B8 (MoveScript_GroupB 0x577280; PSX GAME.EMI#0 US 0x801A96E4,
//             JP 0x801AA6E4, calling US 0x8015D300)
//   DIV-0171  the per-frame voice update (Sprite_UpdateScreen 0x588F20 and
//             Sprite_QueueOverlay 0x5890E0; PSX US 0x8014D184's twin and
//             0x8014D4E0, calling US 0x80161454) and the play that feeds it
//             (Sound_PlayById 0x587900; PSX US 0x8015D71C)
//   DIV-0172  the level and the distance gate (ops BC / BD / D1, PSX US
//             0x8015D7C8 / 0x8015D820; Scena01_Scene09's three calls)
//
// Everything this module keeps is DLL-side (the voices it has changed, a
// level play in progress); the only game word it writes that the PC did not
// is Music_Track under DIV-0170. The PlayStation's own state for the update
// - bit 3 of the sprite's +0 and the voice numbers packed into +0x70 bits
// 8..27 (US 0x8015D71C) - is not written: no PC reader exists for either, and
// both would show in the state hash and the saves.
#include "game/sound_sp.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/sound_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sound_sp {

std::uint8_t g_b8 = 0;        // 1 through PatchBytes("MoveB8Restart")
std::uint8_t g_position = 0;  // 1 through PatchBytes("SpriteVoicePosition")
std::uint8_t g_level = 0;     // 1 through PatchBytes("SoundLevelGate")

namespace {

using sound::Method;

// IDirectSoundBuffer, beyond sound_callees.h's: GetVolume +0x18, GetPan +0x1C,
// SetPan +0x40. The voices' buffers are made with DSBCAPS_CTRLPAN already
// (Snd_BufferDesc's flags 0x180E2, stored by SndBuf_CreateFromWave at
// 0x5A6A4F), so SetPan needs nothing added.
constexpr unsigned kGetVolume = 0x18, kGetPan = 0x1C, kSetPan = 0x40;
using ComGet = long (__stdcall*)(void*, long*);
using ComSet = long (__stdcall*)(void*, long);

// Where the module reads and writes - game memory, or the self-test's own.
struct Places {
    std::uint32_t banks;       // Sound_Banks: a cue's four voice words, a bank's voice entries
    std::uint32_t current;     // Sprite_Current (PSX: the scratchpad's work pointer 0x1F800044)
    std::uint32_t flags2;      // Field_ScriptFlags2, u16 (PSX 0x8014625A: both write bit 0x1000 at the
                               // twins US 0x801B2B6C / PC 0x52F490, `and word ptr [0x905ba4], 0xefff`)
    std::uint32_t mono;        // Rest4f_Setting3, the options' Sound row: Stereo 0 / Mono 1 (PSX 0x80144951)
    std::uint32_t gate;        // Rest4f_Setting5C (PSX 0x80144954, B8's gate; the config block 0x80144950..56
                               // is the PC's 0x903A58..5E, both zeroed at start-up)
    std::uint32_t kind2_x;     // Field_Kind2X (PSX 0x80149308: op D0 at US 0x801AB5F4 passes it as the x)
    std::uint32_t kind2_z;     // Field_Kind2Z (PSX 0x8014930C)
    std::uint32_t objects;     // Sprite_Objects, 30 of 0xA4 (PSX 0x80146888, 30 of 0x98)
    std::uint32_t leader;      // ObjTrio[0] (PSX 0x80145E90)
    std::uint32_t track;       // Music_Track
    std::uint32_t loaded;      // Music_LoadedTrack
    std::uint32_t file;        // Music_File
    std::uint32_t file_size;   // Music_FileSize
    std::uint32_t file_loops;  // Music_FileLoops
    std::uint32_t volume;      // Music_Volume (float)
    std::uint32_t fade_count;  // Music_FadeCount
    std::uint32_t fade_stops;  // Music_FadeStops
};
const Places kPlaces = {
    ::bof3::Img(Sound_Banks_addr),   ::bof3::Img(Sprite_Current_addr), ::bof3::Img(Field_ScriptFlags2_addr), ::bof3::Img(Rest4f_Setting3_addr), ::bof3::Img(Rest4f_Setting5C_addr),
    ::bof3::Img(Field_Kind2X_addr),  ::bof3::Img(Field_Kind2Z_addr),   ::bof3::Img(Sprite_Objects_addr),     ::bof3::Img(ObjTrio_addr),         ::bof3::Img(Music_Track_addr),
    ::bof3::Img(Music_LoadedTrack_addr), ::bof3::Img(Music_File_addr), ::bof3::Img(Music_FileSize_addr),     ::bof3::Img(Music_FileLoops_addr), ::bof3::Img(Music_Volume_addr),
    ::bof3::Img(Music_FadeCount_addr), ::bof3::Img(Music_FadeStops_addr),
};
Places p = kPlaces;

struct Callees {
    void (__cdecl* play_effect)(unsigned short);
    void (__cdecl* music_set_volume)(float);
    void (__cdecl* music_start)(const void*, unsigned, int);
};
const Callees kCallees = {Sound_PlayEffect, Music_SetVolume, Music_Start};
Callees g = kCallees;

unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t Dword(std::uint32_t a) { std::uint32_t v; std::memcpy(&v, At(a), 4); return v; }
std::int32_t Long(const unsigned char* q) { std::int32_t v; std::memcpy(&v, q, 4); return v; }
std::int32_t S16(const unsigned char* q) { std::int16_t v; std::memcpy(&v, q, 2); return v; }
unsigned char* Current() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Dword(p.current))); }

// --- The voices this module has changed ------------------------------------
// A bank entry's buffer is shared by every cue that names that voice, so a
// volume or pan put on it is heard by every later play until it is taken off.
// Each buffer changed here is remembered with what it had before; the next
// play of it through Sound_PlayEffect (BeforePlay), or its end under its
// sprite's update, puts that back - unless something else (Scena13's
// Sound_SetCueVolume) has set the buffer since, which is then left alone.
struct Voice {
    void* buffer;
    unsigned char* owner;   // the sprite whose place it follows (DIV-0171); null: a fixed level (DIV-0172)
    long saved_volume, saved_pan;
    long volume, pan;       // what was put on it last
};
constexpr std::size_t kMaxVoices = 6 * 64;   // a buffer per bank entry: six banks of 64 voices
std::array<Voice, kMaxVoices> g_voices{};
std::size_t g_count = 0;

struct Pending {
    bool on;
    long volume, pan;
};
Pending g_pending{false, 0, 0};

long GetVolume(void* b) { long v = 0; Method<ComGet>(b, kGetVolume)(b, &v); return v; }
long GetPan(void* b) { long v = 0; Method<ComGet>(b, kGetPan)(b, &v); return v; }
void SetVolume(void* b, long v) { Method<ComSet>(b, sound::kSetVolume)(b, v); }
void SetPan(void* b, long v) { Method<ComSet>(b, kSetPan)(b, v); }
bool Playing(void* b) {
    unsigned long status = 0;
    Method<sound::ComStatus>(b, sound::kGetStatus)(b, &status);
    return (status & 1) != 0;
}
long Clamp(long v, long lo, long hi) { return v < lo ? lo : v > hi ? hi : v; }

Voice* Find(void* buffer) {
    for (std::size_t i = 0; i < g_count; ++i)
        if (g_voices[i].buffer == buffer) return &g_voices[i];
    return nullptr;
}
void Forget(Voice* v) { *v = g_voices[--g_count]; }
void Restore(Voice* v) {
    if (GetVolume(v->buffer) == v->volume && GetPan(v->buffer) == v->pan) {
        SetVolume(v->buffer, v->saved_volume);
        SetPan(v->buffer, v->saved_pan);
    }
    Forget(v);
}
Voice* Take(void* buffer, unsigned char* owner) {
    if (g_count == kMaxVoices) bof3::Fatal("sound_sp: more than %u voice buffers changed at once", static_cast<unsigned>(kMaxVoices));
    Voice& v = g_voices[g_count++];
    v.buffer = buffer;
    v.owner = owner;
    v.saved_volume = v.volume = GetVolume(buffer);
    v.saved_pan = v.pan = GetPan(buffer);
    return &v;
}
void Put(Voice* v, long volume, long pan) {
    if (volume != v->volume) SetVolume(v->buffer, volume);
    if (pan != v->pan) SetPan(v->buffer, pan);
    v->volume = volume;
    v->pan = pan;
}

// A PlayStation voice volume pair as a DirectSound volume and pan on top of
// what the buffer had: each side `v` against `full` in hundredths of a
// decibel (amplitude, 2000 log10), never above `full` - DirectSound cannot
// play a buffer louder than its own level - and 0 as silence (-10000). The
// louder side is the volume, the difference the pan (positive quietens the
// left, negative the right).
long Hundredths(std::int32_t v, std::int32_t full) {
    if (v <= 0) return -10000;
    const long h = std::lround(2000.0 * std::log10(static_cast<double>(v) / full));
    return Clamp(h, -10000, 0);
}
void Mix(std::int32_t left, std::int32_t right, std::int32_t full, long saved_volume, long saved_pan, long* volume,
         long* pan) {
    const long l = Hundredths(left, full), r = Hundredths(right, full);
    *volume = Clamp(saved_volume + (l > r ? l : r), -10000, 0);
    *pan = Clamp(saved_pan + r - l, -10000, 10000);
}

// The buffers a cue plays, read as Sound_PlayEffect reads them (sound.cpp,
// original 0x587740): bank (id >> 8) & 0xF counted from 1, unchecked; a word
// of 0 is no voice, voice 0xFF a stop; the entry's buffer at +4.
template <class Fn>
void ForEachBuffer(unsigned short id, Fn fn) {
    const std::uint32_t base = (((id >> 8) & 0xFu) - 1u) * sound::kBankBytes;
    const std::uint32_t cue = p.banks + base + (id & 0xFFu) * 16u;
    for (std::uint32_t n = 0; n < 4; ++n) {
        const std::uint32_t word = Dword(cue + n * 4);
        if (word == 0 || (word & 0xFF) == 0xFF) continue;
        void* const buffer = reinterpret_cast<void*>(static_cast<std::uintptr_t>(
            Dword(p.banks + base + sound::kVoices + (word & 0xFF) * 8 + 4)));
        if (buffer) fn(buffer);
    }
}

}  // namespace

// --- DIV-0171 / DIV-0172: a buffer about to play ---------------------------
void BeforePlay(void* buffer) {
    if (!buffer) return;
    if (Voice* v = Find(buffer)) Restore(v);
    if (!g_pending.on) return;
    Voice* const v = Take(buffer, nullptr);
    Put(v, Clamp(v->saved_volume + g_pending.volume, -10000, 0), Clamp(v->saved_pan + g_pending.pan, -10000, 10000));
}

// --- A bank reload ---------------------------------------------------------
// Snd_LoadBank (display_env.cpp, original 0x587CD0) releases every buffer of
// the bank it replaces before it makes the new ones. A voice remembered here
// for such a buffer must be dropped without a call on it: the first run of
// the combat route after this module was built crashed in DSOUND.dll at the
// battle's end (COMN_SE reloaded, then UpdatePositional's GetStatus on the
// released buffer; 2026-10-23). The remembered volume and pan die with the
// buffer; the new one is made at its own level. Called before the release,
// by its address, so a new buffer made at the same address is a new voice.
void Released(void* buffer) {
    while (Voice* v = Find(buffer)) Forget(v);
}

// --- DIVERGENCE DIV-0171: the voice that follows its sprite ---------------
// PSX US 0x8015D71C (JP 0x8015E10C): the cue queued, then the voice numbers it
// keyed packed into the work record's +0x70 (bits 8, 13, 18, 23) and bit 3 of
// its +0 set, for 0x80161454 to update each frame. Here: the PC's flat play,
// then each buffer the cue played remembered under Sprite_Current. A second
// play by the same sprite adds its buffers, as the PlayStation ORs its voice
// numbers into the word it already has.
void PlayPositional(unsigned short id) {
    unsigned char* const owner = Current();
    g.play_effect(id);
    if (!owner) return;
    ForEachBuffer(id, [owner](void* buffer) {
        if (Find(buffer)) return;   // the cue names one voice twice
        Take(buffer, owner);
    });
}

// PSX US 0x80161454 (JP 0x80161E44), read on the US disc to its jr $ra; the
// work record is Sprite_Current. Two laws:
//   - with bit 0x1000 of Field_ScriptFlags2: by the screen point. Both sides
//     44 (y + 64), 0 above y -64, 0x3FFF from y 0x131; unless mono, a pan p =
//     (x + 40) / 4 percent (0 left of x -40, 100 from x 0x169): right =
//     level p / 100, left = level (100 - p) / 100, truncated.
//   - otherwise by the distance from the kind-2 object (the field's camera
//     focus): d = sqrt(dx^2 + dz^2) on the positions >> 12 (16ths of a
//     cell), both sides 0x37FF - (d >> 4) 10, 0 from 0x37FF; unless mono, the
//     side away from the sprite less level |x - 160| / 480 (0 when that
//     reaches the level). libgte's SquareRoot0 there is a table-and-shift
//     square root; ours is the exact floor, a unit apart at most.
// Each voice the PlayStation finds keyed off (SpuGetKeyStatus 0) is dropped
// from the word; when none is left bit 3 is cleared. Here a buffer that no
// longer plays gets its volume and pan back and is forgotten.
// The levels as heard on the PC: each law's loudest (0x3FFF, 0x37FF) is the
// buffer's own level, the rest that much quieter (Mix).
namespace {
std::uint32_t Isqrt(std::uint32_t n) {
    std::uint32_t r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return r;
}
}  // namespace

void Law(const unsigned char* sprite, std::int32_t* left, std::int32_t* right, std::int32_t* full);
void Law(const unsigned char* sprite, std::int32_t* left, std::int32_t* right, std::int32_t* full) {
    std::uint16_t flags2;
    std::memcpy(&flags2, At(p.flags2), 2);
    const bool mono = *At(p.mono) != 0;
    std::int32_t l, r;
    if (flags2 & 0x1000) {
        *full = 0x3FFF;
        const std::int32_t y = S16(sprite + 0x30);
        std::int32_t v = (y + 0x40) * 0x2C;
        if (y < -0x40) v = 0;
        if (y >= 0x131) v = 0x3FFF;
        l = r = v;
        if (!mono) {
            const std::int32_t x = S16(sprite + 0x2E);
            std::uint32_t pan = x + 0x28 >= 0 ? static_cast<std::uint32_t>(x + 0x28) >> 2 : static_cast<std::uint32_t>(x + 0x2B) >> 2;
            if (x < -0x28) pan = 0;
            if (x >= 0x169) pan = 0x64;
            pan &= 0xFF;
            r = static_cast<std::int16_t>(r) * static_cast<std::int32_t>(pan) / 100;
            l = static_cast<std::int16_t>(l) * static_cast<std::int32_t>(0x64 - pan) / 100;
        }
    } else {
        *full = 0x37FF;
        const std::int32_t dx = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(sprite + 0x34)) - Dword(p.kind2_x)) >> 12;
        const std::int32_t dz = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(sprite + 0x38)) - Dword(p.kind2_z)) >> 12;
        const std::uint32_t sum = static_cast<std::uint32_t>(dx) * static_cast<std::uint32_t>(dx) +
                                  static_cast<std::uint32_t>(dz) * static_cast<std::uint32_t>(dz);
        const std::uint32_t v = (Isqrt(sum) >> 4) * 10;
        l = r = v >= 0x37FF ? 0 : static_cast<std::int32_t>(0x37FF - v);
        if (!mono) {
            const std::int32_t a = S16(sprite + 0x2E) - 0xA0;
            if (a < 0) {
                const std::int32_t level = static_cast<std::int16_t>(r);
                const std::int32_t q = a * level / 480;
                r = (q < 0 ? -q : q) < level ? r - (-a) * level / 480 : 0;
            } else {
                const std::int32_t level = static_cast<std::int16_t>(l);
                const std::int32_t q = a * level / 480;
                l = q < level ? l - q : 0;
            }
        }
    }
    *left = l;
    *right = r;
}

void UpdatePositional() {
    unsigned char* const owner = Current();
    if (!owner) return;
    bool any = false;
    for (std::size_t i = 0; i < g_count && !any; ++i) any = g_voices[i].owner == owner;
    if (!any) return;
    std::int32_t left, right, full;
    Law(owner, &left, &right, &full);
    for (std::size_t i = 0; i < g_count;) {
        Voice* const v = &g_voices[i];
        if (v->owner != owner) { ++i; continue; }
        if (!Playing(v->buffer)) { Restore(v); continue; }   // the slot swapped in is looked at next
        long volume, pan;
        Mix(left, right, full, v->saved_volume, v->saved_pan, &volume, &pan);
        Put(v, volume, pan);
        ++i;
    }
}

// --- DIVERGENCE DIV-0172: the level and the distance gate ------------------
// The level table US 0x80182288 (82 u16 entries, then code pointers), as a
// rule: entry 0 is 0x3FFF, 1..64 fall from 0x3FFF by 0x100 each, 65..81 from
// 0xFF by 0x10 each to 0. 78 of the 82 entries are that; entries 11, 27, 43
// and 59 hold 0x200 more (0x37FF, 0x27FF, 0x17FF, 0x07FF), which no shipped
// call reads (op BC's one call reads 0x28 and 0x3F, the scenario's three
// 0x50, op BD and D1's rings 0x40, 0x31, 0x21, 0x11 and 4) - the rule gives
// the falling value there. Past 81 the PlayStation reads its code pointers:
// fatal here.
std::uint32_t TableLevel(unsigned i);
std::uint32_t TableLevel(unsigned i) {
    if (i == 0) return 0x3FFF;
    if (i <= 64) return 0x3FFF - (i - 1) * 0x100;
    if (i <= 81) return i < 80 ? 0xFF - 0x10 * (i - 64) : 0;
    bof3::Fatal("sound_sp: level table entry %u, past the table's 82 (DIV-0172)", i);
}

namespace {
// SndQueueCue (US 0x8015DF18) gives a cue whose bank entry has bit 7 of its
// byte 1 - every cue of the area banks (bank 2; section 1 of each AREAnnn.EMI)
// - the volume cells D_8018B3D8 / D_8018B3DC through SsUtSetDetVVol, after
// resetting them to 0x17FF unless the cue id has bits 12..15; every shipped
// level call passes a 0x12xx cue. So 0x17FF is such a cue's ordinary level -
// the PC's flat play - and a level is heard against it, never above it.
constexpr std::int32_t kCueLevel = 0x17FF;

void PlayAt(unsigned short id, std::int32_t left, std::int32_t right) {
    long volume, pan;
    Mix(left, right, kCueLevel, 0, 0, &volume, &pan);
    g_pending = {true, volume, pan};
    g.play_effect(id);
    g_pending.on = false;
}
}  // namespace

// PSX US 0x8015D7C8 (JP 0x8015E1B8): the cells from the table's entries
// `left` and `right` (each masked to 16 bits), the cue queued.
void PlayLevel(unsigned short id, unsigned left, unsigned right) {
    if (!g_level) { g.play_effect(id); return; }
    PlayAt(id, static_cast<std::int32_t>(TableLevel(left & 0xFFFF)), static_cast<std::int32_t>(TableLevel(right & 0xFFFF)));
}

// PSX US 0x8015D820 (JP 0x8015E210): six rings round object `index` (cells
// +-22, 18, 12, 8, 6, 2 on both the whole-cell x +0x36 and z +0x3A, signed),
// the leader's (ObjTrio 0's) cell tested against each, inclusive; every ring
// it is inside sets both cells to its entry (0x40, 0x40, 0x31, 0x21, 0x11, 4)
// and queues the cue. The same voices keyed again in the one frame: one
// onset, at the innermost ring's level - so here one play at that level, and
// none outside 22.
void PlayNear(unsigned short id, unsigned index) {
    if (!g_level) { g.play_effect(id); return; }
    index &= 0xFFFF;
    if (index >= 30) bof3::Fatal("sound_sp: object %u past the 30 of Sprite_Objects (DIV-0172, op BD / D1)", index);
    const unsigned char* const leader = At(p.leader);
    const unsigned char* const object = At(p.objects + index * 0xA4);
    const std::int32_t px = S16(leader + 0x36), pz = S16(leader + 0x3A);
    const std::int32_t ox = S16(object + 0x36), oz = S16(object + 0x3A);
    static constexpr struct { std::int32_t reach; unsigned entry; } kRings[] = {
        {22, 0x40}, {18, 0x40}, {12, 0x31}, {8, 0x21}, {6, 0x11}, {2, 0x04}};
    int ring = -1;
    for (int i = 0; i < 6; ++i)
        if (px <= ox + kRings[i].reach && px >= ox - kRings[i].reach && pz <= oz + kRings[i].reach &&
            pz >= oz - kRings[i].reach)
            ring = i;
    if (ring < 0) return;
    const auto level = static_cast<std::int32_t>(TableLevel(kRings[ring].entry));
    PlayAt(id, level, level);
}

bool Routed() { return g_position || g_level; }

// --- DIVERGENCE DIV-0170: op B8 --------------------------------------------
// PSX US 0x8015D300 (JP 0x8015DCF0), called by B8 as (b1, b2, b3, b3):
// SsSepStop(access[b1], b2); unless the gate 0x80144954 is set, the stream
// cells 0x8018B418 / 41A to 0x1FFF and SsSepSetVol(access[b1], b2, b3, b3),
// else SsSepSetVol(.., 0, 0); SsSepPlay(access[b1], b2, 1, 1). The access
// table US 0x80148FC0 is sixteen zeros: every group is the one sequence
// player the music uses (Music_Play's twin US 0x80161C20 starts its tracks
// with this same function at level 0, then fades in). b2 is a sequence of the
// loaded SEP: every music track is group 0 and a sequence 0..3 of its file
// (the track table US 0x80181EB8). The PC's file holds one song, so only
// sequence 0 maps - the song of a track whose file starts with it, which the
// area's own tracks are where the shipped B8s run (below) - and anything
// else is fatal. So: the song in Music_File from its top, at once, at b3 of
// 100 (the PlayStation's song level; the PC's is 127), no fade, any fade
// running dropped. Music_Track is the loaded track again if it was 0xFF (the
// PlayStation's SsSepStop leaves its selection alone, so a later Music_Play
// of the same track does nothing there too).
//
// The shipped B8s (tools' movement-script decode, 3,633 scripts): two, both
// `B8 00 00 64`, area 142's +0x18 script 2 and area 174's +0x10 script 35;
// those areas' zone tracks (Area_StateRecords) are 0x50, 0x6F and 0x8E,
// sequence 0 of their files.
void OpB8(unsigned char group, unsigned char sequence, unsigned char level) {
    (void)group;   // every group is access 0 (above)
    if (sequence != 0)
        bof3::Fatal("sound_sp: op B8 asks for sequence %u of the loaded song file; the PC's file is one song (DIV-0170)",
                    static_cast<unsigned>(sequence));
    void* file;
    std::memcpy(&file, At(p.file), sizeof file);
    if (!file) return;   // no song loaded: the PlayStation's player has nothing either
    const unsigned use = *At(p.gate) ? 0u : level;
    float volume = static_cast<float>(use) * 127.0f / 100.0f;
    if (volume > 127.0f) volume = 127.0f;
    const std::int32_t zero = 0;
    std::memcpy(At(p.fade_count), &zero, 4);
    std::memcpy(At(p.fade_stops), &zero, 4);
    std::memcpy(At(p.volume), &volume, 4);
    g.music_set_volume(volume);
    std::int32_t size, loops;
    std::memcpy(&size, At(p.file_size), 4);
    std::memcpy(&loops, At(p.file_loops), 4);
    g.music_start(file, static_cast<unsigned>(size), loops);
    if (*At(p.track) == 0xFF) {
        std::int32_t loaded;
        std::memcpy(&loaded, At(p.loaded), 4);
        *At(p.track) = static_cast<unsigned char>(loaded);
    }
}

// ============================================================================
// BOF3X_SHADOW=sound_sp: the three repairs on stand-ins - DirectSound buffers
// with a vtable of ours, a bank, the sprites, the leader and the music state
// in this test's own memory, the play and the music calls recorded. The
// expected values are worked by hand from the PlayStation's instructions
// (US 0x80161454, 0x8015D7C8, 0x8015D820, 0x8015D300), not from this code.
// ============================================================================
namespace {

struct FakeBuffer {
    void* const* vtable;
    long volume, pan;
    unsigned long status;
    int sets;   // SetVolume + SetPan calls
};
long __stdcall FbGetVolume(void* b, long* v) { *v = static_cast<FakeBuffer*>(b)->volume; return 0; }
long __stdcall FbGetPan(void* b, long* v) { *v = static_cast<FakeBuffer*>(b)->pan; return 0; }
long __stdcall FbGetStatus(void* b, unsigned long* s) { *s = static_cast<FakeBuffer*>(b)->status; return 0; }
long __stdcall FbSetVolume(void* b, long v) { static_cast<FakeBuffer*>(b)->volume = v; ++static_cast<FakeBuffer*>(b)->sets; return 0; }
long __stdcall FbSetPan(void* b, long v) { static_cast<FakeBuffer*>(b)->pan = v; ++static_cast<FakeBuffer*>(b)->sets; return 0; }
void* g_vtable[0x54 / 4];

struct TestWorld {
    unsigned char banks[2 * sound::kBankBytes];
    unsigned char sprite[0xA4], other[0xA4];
    unsigned char objects[30 * 0xA4];
    unsigned char leader[0x14C];
    std::uint32_t current;
    std::uint16_t flags2;
    unsigned char mono, gate, track, pad;
    std::int32_t kind2_x, kind2_z, loaded, file_size, file_loops, fade_count, fade_stops;
    float volume;
    void* file;
};
TestWorld* g_w = nullptr;
FakeBuffer g_fb[4];
int g_plays = 0, g_starts = 0, g_volumes = 0;
float g_last_volume = -1.0f;
const void* g_start_file = nullptr;

void SetDw(unsigned char* q, std::uint32_t v) { std::memcpy(q, &v, 4); }
void SetS16(unsigned char* q, std::int16_t v) { std::memcpy(q, &v, 2); }
std::uint32_t Addr(const void* q) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(q)); }

// Sound_PlayEffect as far as this module sees it: each buffer the cue plays
// passes BeforePlay, then plays.
void __cdecl TestPlay(unsigned short id) {
    ++g_plays;
    ForEachBuffer(id, [](void* buffer) {
        BeforePlay(buffer);
        static_cast<FakeBuffer*>(buffer)->status = 1;
    });
}
void __cdecl TestSetVolume(float v) { ++g_volumes; g_last_volume = v; }
void __cdecl TestStart(const void* file, unsigned, int) { ++g_starts; g_start_file = file; }

unsigned g_checks = 0, g_bad = 0;
void Check(bool ok, const char* what) {
    ++g_checks;
    if (!ok && ++g_bad <= 12) bof3::Log("shadow      sound_sp self-test MISMATCH: %s", what);
}

void SelfTest() {
    static TestWorld world;
    std::memset(&world, 0, sizeof world);
    g_w = &world;
    g_vtable[kGetVolume / 4] = reinterpret_cast<void*>(&FbGetVolume);
    g_vtable[kGetPan / 4] = reinterpret_cast<void*>(&FbGetPan);
    g_vtable[sound::kGetStatus / 4] = reinterpret_cast<void*>(&FbGetStatus);
    g_vtable[sound::kSetVolume / 4] = reinterpret_cast<void*>(&FbSetVolume);
    g_vtable[kSetPan / 4] = reinterpret_cast<void*>(&FbSetPan);
    for (FakeBuffer& b : g_fb) b = {g_vtable, 0, 0, 0, 0};
    g_fb[3].volume = -300;   // a buffer with a level of its own (Sound_SetCueVolume's)
    // Bank 1, cue 5: voices 2 and 3, a stop (0xFF) and an empty word; cue 6:
    // voice 4 (buffer 3) alone. Voice entries hold the fake buffers.
    unsigned char* const bank = world.banks;
    SetDw(bank + 5 * 16 + 0, 0x00001002u);
    SetDw(bank + 5 * 16 + 4, 0x000011FFu);
    SetDw(bank + 5 * 16 + 8, 0);
    SetDw(bank + 5 * 16 + 12, 0x80001203u);
    SetDw(bank + 6 * 16 + 0, 0x00001304u);
    SetDw(bank + sound::kVoices + 2 * 8 + 4, Addr(&g_fb[0]));
    SetDw(bank + sound::kVoices + 3 * 8 + 4, Addr(&g_fb[1]));
    SetDw(bank + sound::kVoices + 4 * 8 + 4, Addr(&g_fb[3]));
    p = {Addr(bank), Addr(&world.current), Addr(&world.flags2), Addr(&world.mono), Addr(&world.gate),
         Addr(&world.kind2_x), Addr(&world.kind2_z), Addr(world.objects), Addr(world.leader), Addr(&world.track),
         Addr(&world.loaded), Addr(&world.file), Addr(&world.file_size), Addr(&world.file_loops), Addr(&world.volume),
         Addr(&world.fade_count), Addr(&world.fade_stops)};
    g = {TestPlay, TestSetVolume, TestStart};
    const std::uint8_t keep[3] = {g_b8, g_position, g_level};
    g_b8 = g_position = g_level = 1;
    g_count = 0;

    std::int32_t l, r, full;
    // The screen law (flags2 bit 0x1000), at (x 0, y 0): level 64 * 44 = 2816,
    // pan (0 + 40) >> 2 = 10: right 281, left 2534; mono both 2816; y -65 0;
    // y 0x131 0x3FFF; x 0x169 all right.
    world.flags2 = 0x1000;
    SetS16(world.sprite + 0x2E, 0); SetS16(world.sprite + 0x30, 0);
    Law(world.sprite, &l, &r, &full);
    Check(l == 2534 && r == 281 && full == 0x3FFF, "screen law at (0, 0)");
    world.mono = 1; Law(world.sprite, &l, &r, &full); world.mono = 0;
    Check(l == 2816 && r == 2816, "screen law, mono");
    SetS16(world.sprite + 0x30, -0x41); Law(world.sprite, &l, &r, &full);
    Check(l == 0 && r == 0, "screen law above y -64");
    SetS16(world.sprite + 0x30, 0x131); SetS16(world.sprite + 0x2E, 0x169); Law(world.sprite, &l, &r, &full);
    Check(l == 0 && r == 0x3FFF, "screen law at the bottom right");
    // The field law: on the focus, centre of the screen: both 0x37FF. At x 0
    // (160 left): right 0x37FF - 160 * 0x37FF / 480 = 14335 - 4778 = 9557.
    // 32 cells away on x (dx 512, d 512, (512 >> 4) * 10 = 320): 14015 both.
    world.flags2 = 0;
    SetDw(world.sprite + 0x34, 0x00200000u); SetDw(world.sprite + 0x38, 0x00100000u);
    world.kind2_x = 0x00200000; world.kind2_z = 0x00100000;
    SetS16(world.sprite + 0x2E, 0xA0);
    Law(world.sprite, &l, &r, &full);
    Check(l == 0x37FF && r == 0x37FF && full == 0x37FF, "field law on the focus");
    SetS16(world.sprite + 0x2E, 0); Law(world.sprite, &l, &r, &full);
    Check(l == 0x37FF && r == 9557, "field law at screen x 0");
    SetS16(world.sprite + 0x2E, 0x140); Law(world.sprite, &l, &r, &full);
    Check(l == 9557 && r == 0x37FF, "field law at screen x 320");
    SetDw(world.sprite + 0x34, 0x00400000u); SetS16(world.sprite + 0x2E, 0xA0); Law(world.sprite, &l, &r, &full);
    Check(l == 14015 && r == 14015, "field law 32 cells away");

    // DIV-0171: a play under the sprite remembers buffers 0 and 1 (voices 2
    // and 3; the stop and the empty word nothing); the update puts the law on
    // them; a buffer that has stopped gets its level back; a flat play of a
    // remembered buffer takes it back.
    world.current = Addr(world.sprite);
    SetDw(world.sprite + 0x34, 0x00200000u);
    SetS16(world.sprite + 0x2E, 0);   // left of centre: the right side quieter
    g_plays = 0;
    PlayPositional(0x105);
    Check(g_plays == 1 && g_count == 2, "PlayPositional: one play, two buffers remembered");
    UpdatePositional();
    const long expect_pan = std::lround(2000.0 * std::log10(9557.0 / 0x37FF));   // the right side down
    Check(g_fb[0].volume == 0 && g_fb[0].pan == expect_pan && g_fb[1].pan == expect_pan, "update: volume and pan");
    world.current = Addr(world.other);
    const int sets = g_fb[0].sets;
    UpdatePositional();
    Check(g_fb[0].sets == sets, "update for another sprite leaves them");
    world.current = Addr(world.sprite);
    g_fb[1].status = 0;
    UpdatePositional();
    Check(g_count == 1 && g_fb[1].pan == 0 && g_fb[1].volume == 0, "a stopped voice gets its level back");
    TestPlay(0x105);   // a flat play (Sound_PlayById with no sprite, or any Sound_PlayEffect)
    Check(g_count == 0 && g_fb[0].pan == 0, "a flat play takes the buffer back");
    g_fb[0].status = g_fb[1].status = 0;

    // A level against a buffer with its own (-300): the pan rides on it, and
    // something else setting the buffer meanwhile is left alone.
    world.current = Addr(world.sprite);
    PlayPositional(0x106);
    UpdatePositional();
    Check(g_fb[3].volume == -300 && g_fb[3].pan == expect_pan, "the law on top of the buffer's own level");
    g_fb[3].volume = -500;   // Sound_SetCueVolume meanwhile
    TestPlay(0x106);
    Check(g_count == 0 && g_fb[3].volume == -500, "a level set since is not undone");
    g_fb[3] = {g_vtable, 0, 0, 0, 0};

    // DIV-0172: the table rule at the entries the shipped calls read.
    Check(TableLevel(0) == 0x3FFF && TableLevel(0x28) == 0x18FF && TableLevel(0x3F) == 0x1FF && TableLevel(0x40) == 0xFF &&
              TableLevel(0x31) == 0xFFF && TableLevel(0x21) == 0x1FFF && TableLevel(0x11) == 0x2FFF &&
              TableLevel(4) == 0x3CFF && TableLevel(0x50) == 0 && TableLevel(65) == 0xEF && TableLevel(79) == 0xF,
          "the level table's rule");
    // Op BC's call: left 0x18FF (above 0x17FF: the buffer's level), right
    // 0x1FF (-21.6 dB): pan -2161 (right down).
    PlayLevel(0x105, 0x28, 0x3F);
    const long bc_pan = -std::lround(2000.0 * std::log10(static_cast<double>(0x17FF) / 0x1FF));
    Check(g_count == 2 && g_fb[0].volume == 0 && g_fb[0].pan == bc_pan, "PlayLevel: op BC's level");
    PlayLevel(0x105, 0x50, 0x50);
    Check(g_fb[0].volume == -10000 && g_fb[1].volume == -10000, "PlayLevel: the scenario's level 0 is silence");
    TestPlay(0x105);
    Check(g_count == 0 && g_fb[0].volume == 0 && g_fb[0].pan == 0, "a flat play after a level play");
    // The rings: object 3 at cell (40, 40); the leader 23 away: nothing; 20
    // away: 0xFF; 10: 0xFFF; 5: 0x2FFF (the buffer's level).
    SetS16(world.objects + 3 * 0xA4 + 0x36, 40); SetS16(world.objects + 3 * 0xA4 + 0x3A, 40);
    struct { std::int16_t x, z; int plays; long volume; } rings[] = {
        {63, 40, 0, 0}, {40, 17, 0, 0}, {60, 40, 1, Hundredths(0xFF, 0x17FF)}, {30, 50, 1, Hundredths(0xFFF, 0x17FF)},
        {45, 35, 1, 0}, {41, 39, 1, 0}};
    for (const auto& c : rings) {
        SetS16(world.leader + 0x36, c.x); SetS16(world.leader + 0x3A, c.z);
        g_plays = 0;
        PlayNear(0x105, 3);
        Check(g_plays == c.plays && (c.plays == 0 || g_fb[0].volume == c.volume), "PlayNear's rings");
        TestPlay(0x105);
    }
    Check(g_count == 0, "nothing remembered after the rings");

    // DIV-0170: B8 00 00 64 on a stopped song (track 0xFF, loaded 0x50, a fade
    // running): level 127, started from the file, the fade dropped, the
    // track back; the gate: level 0.
    int file_byte = 0;
    world.file = &file_byte; world.track = 0xFF; world.loaded = 0x50; world.fade_count = 7; world.fade_stops = 1;
    g_starts = g_volumes = 0;
    OpB8(0, 0, 0x64);
    Check(g_starts == 1 && g_start_file == &file_byte && g_last_volume == 127.0f && world.volume == 127.0f &&
              world.fade_count == 0 && world.fade_stops == 0 && world.track == 0x50,
          "op B8 restarts the song at its level");
    world.gate = 1; world.track = 0x12;
    OpB8(3, 0, 0x64);
    Check(g_last_volume == 0.0f && world.track == 0x12, "op B8 under the gate: level 0, the track kept");
    world.file = nullptr; g_starts = 0;
    OpB8(0, 0, 0x64);
    Check(g_starts == 0, "op B8 with no song loaded");

    // Everything back.
    g_b8 = keep[0]; g_position = keep[1]; g_level = keep[2];
    g_count = 0;
    g_pending.on = false;
    p = kPlaces;
    g = kCallees;
    g_w = nullptr;
    bof3::Log("shadow      sound_sp self-test: %u checks, %u MISMATCHES (the laws, the voice bookkeeping, the levels, "
              "the rings, op B8)", g_checks, g_bad);
    if (g_bad) bof3::Fatal("sound_sp self-test: %u of %u checks differ", g_bad, g_checks);
}

}  // namespace

void Arm() {
    if (bof3::WantsShadow("sound_sp")) SelfTest();
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("MoveB8Restart", Addr(&g_b8), &was, &is, 1);
    bof3::Log("DIV-0170    op B8 restarts the song from its top at the script's level (on unless the line above says OFF)");
    bof3::PatchBytes("SpriteVoicePosition", Addr(&g_position), &was, &is, 1);
    bof3::Log("DIV-0171    Sound_PlayById's voices follow their sprite: volume and pan by its place (on unless the line "
              "above says OFF)");
    bof3::PatchBytes("SoundLevelGate", Addr(&g_level), &was, &is, 1);
    bof3::Log("DIV-0172    ops BC / BD / D1 and the scenario's level cues play at their level and only near their object "
              "(on unless the line above says OFF)");
}

}  // namespace sound_sp
