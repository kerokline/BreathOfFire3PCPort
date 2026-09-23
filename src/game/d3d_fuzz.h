// The vertex-block fuzz: a harness for differential fuzzing of the Direct3D
// draw handlers under the draw 0x59EE50 (docs/glyph-draw.md section 5).
//
// Every one of those handlers does the same three things: fills some of the
// four 0x20-byte D3DTLVERTEX records at D3d_Vertices 0x7CA958, calls a few
// helpers (D3d_PrimColor, D3d_SetBlend, D3d_SetShadeMode, a texture lookup),
// and ends in COM calls on the IDirect3DDevice3 at D3d_Device 0x7CC350. So a
// handler is compared against a byte-copy of Capcom's by:
//
//   - a Log of every call out, in order: the helpers' recording stand-ins
//     (the user's own, through Record) and the COM methods of a FAKE device
//     that DeviceSwap puts in D3d_Device for the length of the fuzz;
//   - a snapshot of the vertices each DrawPrimitive is handed, taken at the
//     moment of the call (so a store after the draw, or a draw of the wrong
//     block, shows even when the block ends up right);
//   - the vertex block itself after the call, and the handler's return value
//     (a COM method returns a value derived from its position in the log, so
//     "returns what DrawPrimitive returned" is checkable).
//
// The fake device's vtable has 64 slots. Those Arity() in d3d_fuzz.cpp knows record
// (0x100 + slot, the arguments after `this`) and return that value; every
// other slot ends the process naming its number, so a handler that calls a
// method nobody thought of is refused, not silently answered. To cover a new
// method: add its slot and arity (the count of arguments INCLUDING `this`:
// stdcall, the callee pops them) to Arity() in d3d_fuzz.cpp. Slots are
// IDirect3DDevice3's (d3d.h order): 9 BeginScene, 10 EndScene,
// 22 SetRenderState, 28 DrawPrimitive, 38 SetTexture, 40 SetTextureStageState.
//
// Nothing here runs in game: only a start-up self-test under BOF3X_SHADOW
// uses it, before BOF3.exe's C runtime is up - so no CRT of the game's, and
// this file's random numbers are its own.
#pragma once

#include <cstdint>

namespace d3d_fuzz {

constexpr std::uint32_t kDevice = 0x7CC350;      // D3d_Device, an IDirect3DDevice3 *
constexpr std::uint32_t kVertices = 0x7CA958;    // D3d_Vertices, 4 x D3DTLVERTEX
constexpr std::uint32_t kVertexBytes = 0x80;
constexpr std::uint32_t kScaleX = 0x7C9F4C;      // D3d_ScaleX, float
constexpr std::uint32_t kScaleY = 0x7C9F48;      // D3d_ScaleY, float

// One call out. `what` is 0x100 + slot for a COM method; a user's stand-ins
// use numbers below 0x100. `snap` is the DrawPrimitive snapshot's index, or -1.
struct Call {
    std::uint32_t what;
    std::uint32_t a[7];
    int snap;
};

constexpr unsigned kMaxCalls = 48;
constexpr unsigned kMaxSnaps = 4;
constexpr unsigned kSnapBytes = kVertexBytes;

struct Log {
    Call calls[kMaxCalls];
    unsigned n;                                   // may exceed kMaxCalls: counted, not stored
    unsigned char snaps[kMaxSnaps][kSnapBytes];   // what each DrawPrimitive was handed
    unsigned n_snaps;
    void Clear();
};

// Where Record and the fake device write. Set it before each pass.
extern Log* g_log;

// Appends a call to *g_log.
void Record(std::uint32_t what, std::uint32_t a0 = 0, std::uint32_t a1 = 0, std::uint32_t a2 = 0,
            std::uint32_t a3 = 0, std::uint32_t a4 = 0, std::uint32_t a5 = 0, std::uint32_t a6 = 0);

// What the fake device's methods return: a function of the log position, so
// equal logs give equal return values and a return taken from the wrong call
// shows.
std::uint32_t ComResult(unsigned position);

// Puts the fake device in D3d_Device for its lifetime, and back after.
class DeviceSwap {
public:
    DeviceSwap();
    ~DeviceSwap();
    DeviceSwap(const DeviceSwap&) = delete;
    DeviceSwap& operator=(const DeviceSwap&) = delete;

private:
    std::uint32_t saved_;
};

// The fake device itself, for a handler's own checks (its address is what
// every recorded COM call's `this` must have been; the log leaves `this` out).
void* FakeDevice();

// Compares two logs call by call and snapshot by snapshot. On a difference,
// writes one line naming it to `why` (at least 160 bytes) and returns false.
// `same_snap` compares one pair of snapshots; null means byte-for-byte.
using SnapCompare = bool (*)(const unsigned char* ours, const unsigned char* theirs);
bool SameLog(const Log& ours, const Log& theirs, char* why, SnapCompare same_snap = nullptr);

// A small xorshift of our own (the game's CRT is not up at start-up).
void Seed(std::uint32_t seed);
std::uint32_t Next();
std::uint32_t Pick(const std::uint32_t* values, unsigned n);   // one of them

}  // namespace d3d_fuzz
