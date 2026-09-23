// The surface fuzz: a fake DirectDraw for differential fuzzing of the code that
// builds textures through DirectDraw surfaces, without a window
// (docs/tex-page.md section 5). The Direct3D device's twin is d3d_fuzz.h.
//
// The port's DirectDraw is DirectX 6's: an IDirectDraw4 at Dd_DirectDraw
// 0x7CC334, IDirectDrawSurface4s, DDSURFACEDESC2 (0x7C bytes) and the
// IDirect3DTexture2 each texture surface answers QueryInterface with. This file
// fakes all three:
//
//   - a fake IDirectDraw4 whose CreateSurface (+0x18) makes fake surfaces from
//     the descriptor it is handed, and records a copy of that descriptor;
//   - fake surfaces whose Lock (+0x64) hands out a buffer the fuzz owns, at a
//     pitch the fuzz chooses, and fills the descriptor the way DirectDraw does
//     (and refuses one whose dwSize is not 0x7C, as DirectDraw does); Unlock
//     (+0x80); Blt (+0x14), which copies pixels when the two rectangles are the
//     same size and the surfaces the same depth; BltFast (+0x1C); SetColorKey
//     (+0x74); GetSurfaceDesc (+0x58); Flip (+0x2C), IsLost (+0x60), Restore
//     (+0x6C); QueryInterface (+0) for IID_IDirect3DTexture2; AddRef, Release;
//   - the fake IDirect3DTexture2 behind each surface: QueryInterface, AddRef,
//     Release, GetHandle (+0xC), PaletteChanged (+0x10), Load (+0x14, copies
//     the pixels).
// Every call is recorded with its arguments; any other method ends the process
// naming its slot, so a function that calls a method nobody thought of is
// refused, not answered. To cover one: add it to the object's table in
// ddraw_fuzz.cpp.
//
// TWO PASSES, ONE SET OF OBJECTS. A differential fuzz runs Capcom's copy and
// ours from the same state and compares. The fake objects are one static set -
// the same addresses in both passes - so a surface pointer the code under test
// stores in game memory compares equal. Only their pixels are per pass: each
// surface has a buffer in arena 0 and another in arena 1, filled alike, and
// Lock hands out the one of the current pass. SamePixels compares them.
//
//   per round:  Reset(seed); MakeSurface(...) for what exists before the call
//               (the staging surface, a cache entry's surfaces); set up memory;
//   pass 0:     BeginPass(0); run Capcom's copy;  (save what it returned)
//   pass 1:     restore memory; BeginPass(1); run ours;
//   compare:    SameLog(ours, theirs), SamePixels(), memory, the return.
//
// Surfaces are identified in the log by their index (kSurfaceId + index; a
// texture kTextureId + index); a pointer argument that is neither is recorded
// as its value. Nothing here runs in game: only start-up self-tests under
// BOF3X_SHADOW use it, before BOF3.exe's C runtime is up.
#pragma once

#include <cstdint>

namespace ddraw_fuzz {

using U = std::uint32_t;

constexpr U kDirectDraw = 0x7CC334;   // Dd_DirectDraw, an IDirectDraw4 *
constexpr U kStage = 0x7CC344;        // Dd_StageSurface, an IDirectDrawSurface4 *

// What a recorded call is: 0x300 + slot for the IDirectDraw4, 0x400 + slot for
// a surface, 0x500 + slot for a texture. A user's own stand-ins use numbers
// below 0x100.
constexpr U kDirectDrawCall = 0x300, kSurfaceCall = 0x400, kTextureCall = 0x500;
// IDirectDraw4 slot
constexpr U kCreateSurface = 6;
// IDirectDrawSurface4 slots
constexpr U kQueryInterface = 0, kAddRef = 1, kRelease = 2, kBlt = 5, kBltFast = 7, kFlip = 11,
            kGetSurfaceDesc = 22, kIsLost = 24, kLock = 25, kRestore = 27, kSetColorKey = 29, kUnlock = 32;
// IDirect3DTexture2 slots
constexpr U kGetHandle = 3, kPaletteChanged = 4, kLoad = 5;

constexpr U kSurfaceId = 0x5000;      // a surface's id in the log: kSurfaceId + index
constexpr U kTextureId = 0x6000;      // its texture's
constexpr U kNullRect = 0xFFFFFFFFu;  // a null RECT *: recorded as four of these

// DirectDraw's results, as the fake returns them.
constexpr U kOk = 0;
constexpr U kInvalidParams = 0x80070057u;   // DDERR_INVALIDPARAMS: a descriptor's dwSize not 0x7C
constexpr U kNoInterface = 0x80004002u;     // E_NOINTERFACE
constexpr U kSurfaceBusy = 0x887601AEu;     // DDERR_SURFACEBUSY: Lock of a locked surface
constexpr U kNotLocked = 0x887601E8u;       // DDERR_NOTLOCKED: Unlock of an unlocked one
constexpr U kSurfaceLost = 0x887601C2u;     // DDERR_SURFACELOST: what a planned failure returns by default

// --- the log -------------------------------------------------------------------

struct Call {
    U what;
    U a[12];
};
constexpr unsigned kMaxCalls = 40;
constexpr unsigned kDescBytes = 0x7C;
constexpr unsigned kMaxDescs = 4;

struct Log {
    Call calls[kMaxCalls];
    unsigned n;                                 // may exceed kMaxCalls: counted, not stored
    unsigned char descs[kMaxDescs][kDescBytes]; // each CreateSurface's descriptor, as handed in
    unsigned n_descs;
    void Clear();
};

// Where every call is recorded. Set it before each pass.
extern Log* g_log;

void Record(U what, U a0 = 0, U a1 = 0, U a2 = 0, U a3 = 0, U a4 = 0, U a5 = 0, U a6 = 0, U a7 = 0,
            U a8 = 0, U a9 = 0, U a10 = 0, U a11 = 0);

// Compares two logs call by call and descriptor by descriptor. On a
// difference writes one line to `why` (at least 200 bytes) and returns false.
bool SameLog(const Log& ours, const Log& theirs, char* why);

// Called after every recorded call of the fake objects (with the call's
// `what`), for a fuzz that disturbs memory between the calls the code under
// test makes - to order its reads against its calls. Null for none. It must be
// deterministic in what it is given and in the log's length, so that both
// passes are disturbed alike.
using Disturb = void (*)(U what);
void SetDisturb(Disturb d);

// --- the objects ---------------------------------------------------------------

constexpr unsigned kMaxSurfaces = 8;
constexpr U kSlotBytes = 0x110000;   // each surface's buffer, per pass: 256 rows at a pitch of up to 4,096 and more

// Clears every surface and failure plan's counters, forgets the pitch plan,
// and sets the fill seed. Call before each round.
void Reset(U seed);

// A surface that exists before the call under test, e.g. the staging surface.
// `bytes_per_pixel` 2 or 4; `pitch` 0 means width * bytes_per_pixel. Returns
// the IDirectDrawSurface4 pointer (the same in both passes).
void* MakeSurface(U width, U height, U bytes_per_pixel, U pitch);

// The pitch each surface CreateSurface makes gets: the k-th one made in a pass
// takes pitches[k] (k counts from 0 for the first one CreateSurface makes);
// past the end of the list, width * bytes_per_pixel. Kept until Reset.
void SetPitches(const U* pitches, unsigned n);

// Makes the `nth` (from 0, counted per pass) call of `what` (kSurfaceCall +
// kLock, kDirectDrawCall + kCreateSurface, ...) return `result` and do nothing
// else - it is still recorded. Up to four plans; kept until Reset.
void FailAt(U what, unsigned nth, U result = kSurfaceLost);

// Starts a pass: pass 0 or 1. Every surface MakeSurface made is put back as it
// was made (unlocked, its buffer in this pass's arena filled from the seed),
// every surface a previous pass's CreateSurface made is forgotten, and the
// per-pass counters restart.
void BeginPass(int pass);

// Compares every surface's buffer between the two arenas, over the bytes a
// surface of its pitch can have written (256 rows and a margin). On a
// difference writes one line to `why` and returns false.
bool SamePixels(char* why);

// The fake IDirectDraw4.
void* FakeDirectDraw();

// What a surface or texture pointer is in the log: kSurfaceId + index,
// kTextureId + index, or the pointer's value.
U Id(const void* p);

// Where `p` lies in the current pass's buffers: the surface's index and the
// offset into its buffer. False if in none.
bool Locate(const void* p, U* surface, U* offset);

// The current pass's buffer of surface `index` (for a fuzz that reads back).
unsigned char* Pixels(unsigned index);

// Surfaces made so far this pass, MakeSurface's and CreateSurface's.
unsigned SurfaceCount();

// Puts `value` in the dword at `address` for its lifetime, and back after -
// the fake IDirectDraw4 in Dd_DirectDraw, a staging surface in
// Dd_StageSurface.
class GlobalSwap {
public:
    GlobalSwap(U address, const void* value);
    ~GlobalSwap();
    GlobalSwap(const GlobalSwap&) = delete;
    GlobalSwap& operator=(const GlobalSwap&) = delete;

private:
    U address_, saved_;
};

}  // namespace ddraw_fuzz
