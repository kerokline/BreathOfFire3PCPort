// Three spell overlays compiled into the exe, round nine group S14
// (docs/magic_s14.md): the PSX's MAGIC064, MAGIC065 and MAGIC066.EMI,
// Magic_Rows rows 77, 70 and 17. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Weretiger, Pilfer and Tsunami; the names
// below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC064 0x4B3F00..0x4B54A8: the acting party member posed, the field
//     view re-centred on it (0x4B5350 rebuilds the map view round the kind-2
//     point), a burst of line streaks from a pool of 0xC0 records (0x683288,
//     the current one at 0x684788), a copy of the member that spawns four
//     textured images of it and a backdrop dim; its form's DAT file loaded
//     while two sprite pairs cross-fade, its palette and CLUT redone, the view
//     put back;
//   - MAGIC065: one function, 0x4B58F0, the item-name copy into Text_Records
//     that Pilfer's and Steal's successful rolls call (Pilfer's other eight
//     functions are round eight's, magic_fx_reached.cpp);
//   - MAGIC066 0x4B59A0..0x4B66C0: a wave of 127 textured quads and two
//     gouraud ones up a sine across the field, then rings of four textured
//     quads round every live enemy and party member at three heights.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s14.h"

#include "game/draw_pool.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F; words by function) and the four SVECTORs
// of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// MAGIC064's streak pool in .bss: 0xC0 records of 0x1C bytes (+0 live, +1 the
// step, +2 an angle word, +4 / +8 the centre, +0xC / +0x10 two radii 16.16,
// +0x14 / +0x18 their growth), the record being run, and MAGIC066's four ring
// phases after it.
constexpr std::uint32_t kPool = 0x683288;
constexpr std::uint32_t kPoolStride = 0x1C;
constexpr unsigned kPoolCount = 0xC0;
constexpr std::uint32_t kPoolCurrent = 0x684788;
constexpr std::uint32_t kRingPhases = 0x68478C;

// Overlay .data read in place (addresses only; the values are the exe's).
constexpr std::uint32_t kStreakOffsets = 0x65ABF8;    // 2 signed bytes (x, y) a facing
constexpr std::uint32_t kImageUV = 0x65AC10;          // 4 bytes an image: u at +0, v at +2
constexpr std::uint32_t kRingOffsets = 0x65AC54;      // 3 of (x, z, y) dwords

constexpr std::uint32_t kFrameCounter = 0x937F94;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `cdq / and edx, 2^n - 1 / add / sar n`: a signed divide by 2^n toward zero.
int DivPow2(int v, unsigned n) {
    const std::uint32_t bias = static_cast<std::uint32_t>(v >> 31) & ((1u << n) - 1);
    return static_cast<int>(static_cast<std::uint32_t>(v) + bias) >> n;
}

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The party records by the battle index, unchecked (the originals index
// ObjTrio by the actor byte whatever it holds).
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* PoolRecord(unsigned i) { return Mem(kPool + i * kPoolStride); }
unsigned char* PoolCurrent() { return Pointer(kPoolCurrent); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }

// Capcom's phase handlers a table holds, unnamed, in the engine: 0x492750
// steps +1 on; 0x437CC0 is a bare ret (a step that waits for a child to move
// +2 on).
constexpr std::uint32_t kStepOn = bof3::addr::Effect_StateNext;   // 0x492750, ours since round thirteen
constexpr std::uint32_t kNothing = bof3::addr::BareRet;

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The callees with the arguments the originals push. Gte_RotTransPers4 gets
// the depth and flag pointers the originals pass; ours reads the first.
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S14_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +0x18, +0x28, +0x38.
void Rtp4(unsigned char* prim) {
    long p, flag;
    S14_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &p, &flag);
}
// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

// The 0x80 bytes of a party record into a task slot: rep movsd, 0x20 dwords
// forward (a record and a slot never overlap).
void CopyRecord(unsigned char* dst, const unsigned char* src) {
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(dst + k, Long(src + k));
}

}  // namespace

#define S14_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC064 (row 77, Weretiger read one id down)

// original 0x4B3F00: the kind-2 task. A three-entry stack table by +1: the
// engine's 0x492750 (+1 on), Weretiger_Run, Weretiger_End.
S14_EXPORT void __cdecl Weretiger_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {kStepOn, bof3::addr::Weretiger_Run, bof3::addr::Weretiger_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Weretiger_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B3F30: a twelve-entry stack table by +2: _Pose, _WaitPose,
// _FocusActor, _Burst, a bare ret (0x437CC0: the burst child moves +2 on),
// _Copy, _Release, the bare ret again (the copy child moves it), _LoadForm,
// _FadeIn, _WaitScript, _Return.
S14_EXPORT void __cdecl Weretiger_Run(void) {
    static constexpr std::uint32_t kSteps[12] = {
        bof3::addr::Weretiger_Pose,     bof3::addr::Weretiger_WaitPose, bof3::addr::Weretiger_FocusActor,
        bof3::addr::Weretiger_Burst,    kNothing,                       bof3::addr::Weretiger_Copy,
        bof3::addr::Weretiger_Release,  kNothing,                       bof3::addr::Weretiger_LoadForm,
        bof3::addr::Weretiger_FadeIn,   bof3::addr::Weretiger_WaitScript, bof3::addr::Weretiger_Return};
    const unsigned phase = Sc()[2];
    if (phase >= 12) PastTable("Weretiger_Run", phase, 12);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B3FB0: with Sprite_Current the acting member's record (ObjTrio
// by the actor byte 0x904B34), its animation +8 + 0x38 ensured; the task back;
// sound effect 0x100; +0xB 0, +2 on.
S14_EXPORT void __cdecl Weretiger_Pose(void) {
    unsigned char* const self = Sc();
    unsigned char* const record = PartyRecord(ActorIndex());
    Sprite_Current = record;
    MH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(record[8] + 0x38));
    Sprite_Current = self;
    MH_CALL(Sound_PlayEffect)(0x100);
    Sc()[0xB] = 0;
    Inc(Sc()[2]);
}

// original 0x4B4010: the acting member's script ticked once (Sprite_Current
// its record, then the task back); not at its end: +9 0x10; at its end: +2 on.
S14_EXPORT void __cdecl Weretiger_WaitPose(void) {
    unsigned char* const self = Sc();
    Sprite_Current = PartyRecord(ActorIndex());
    const unsigned char done = MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    if (done == 0) {
        self[9] = 0x10;
        return;
    }
    Inc(self[2]);
}

// original 0x4B4060: +9 down; when it was 0: the task's position the field's
// kind-2 point and its height the view's elevation; the kind-2 point moved to
// the acting member's +0x34 / +0x38, the elevation the ground there
// (AreaMap_Elevation), the map view rebuilt round it (0x4B5350); a child (kind
// 1, 0x3A) with +0x80 this task and +1 3 (the backdrop dim); +9 1, +0xA 1, +2
// on.
S14_EXPORT void __cdecl Weretiger_FocusActor(void) {
    {
        unsigned char* const s = Sc();
        const unsigned char was = s[9];
        s[9] = static_cast<unsigned char>(was - 1);
        if (was != 0) return;
    }
    SetLong(Sc() + 0x34, Field_Kind2X);
    SetLong(Sc() + 0x38, Field_Kind2Z);
    SetWord(Sc() + 0x3E, U(MapView_Elevation) & 0xFFFF);
    {
        const unsigned char* const record = PartyRecord(ActorIndex());
        const std::int32_t x = Long(record + 0x34);
        const std::int32_t z = Long(record + 0x38);
        Field_Kind2X = x;
        Field_Kind2Z = z;
        const long ground = MH_CALL(AreaMap_Elevation)(x, z);
        MapView_Elevation = static_cast<short>(ground);
    }
    Call0(bof3::addr::Weretiger_ResetMapView);
    const unsigned slot = NewTask(0x3A);
    unsigned char* const child = TaskSlot(slot);
    unsigned char* const s = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = 3;
    s[9] = 1;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4B4140: while +0xA is set, the view's runs placed
// (MapView_PlaceRuns) and +0xA 0; +9 down; when it was 0: a child (kind 1,
// 0x3A) with +0x80 this task and +1 1 (the streak burst), bit 0 of +0xB
// cleared, sound effect 0x101, +2 on.
S14_EXPORT void __cdecl Weretiger_Burst(void) {
    if (Sc()[0xA] != 0) {
        MH_CALL(MapView_PlaceRuns)();
        Sc()[0xA] = 0;
    }
    {
        unsigned char* const s = Sc();
        const unsigned char was = s[9];
        s[9] = static_cast<unsigned char>(was - 1);
        if (was != 0) return;
    }
    const unsigned slot = NewTask(0x3A);
    unsigned char* const child = TaskSlot(slot);
    unsigned char* const s = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = 1;
    s[0xB] &= 0xFE;
    MH_CALL(Sound_PlayEffect)(0x101);
    Inc(Sc()[2]);
}

// original 0x4B41C0: a child (kind 1, 0x3A) whose first 0x80 bytes are the
// acting member's record, +0x80 this task, +1 +2 +3 0, +6 1, +5 0x3A (the
// copy that spawns the images); +9 1, +2 on.
S14_EXPORT void __cdecl Weretiger_Copy(void) {
    const unsigned slot = NewTask(0x3A);
    unsigned char* const child = TaskSlot(slot);
    CopyRecord(child, PartyRecord(ActorIndex()));
    unsigned char* const s = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = 0;
    child[2] = 0;
    child[3] = 0;
    child[6] = 1;
    child[5] = 0x3A;
    s[9] = 1;
    Inc(Sc()[2]);
}

// original 0x4B4250: +9 down to 0; at 0 the owner's tint released, the
// owner's +0 bit 0x40 set, +9 0, +2 on.
S14_EXPORT void __cdecl Weretiger_Release(void) {
    {
        unsigned char* const s = Sc();
        if (s[9] != 0) {
            Dec(s[9]);
            return;
        }
    }
    MH_CALL(Sprite_ReleaseTint)(Owner());
    Owner()[0] |= 0x40;
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

namespace {

// The four sprites of the cross-fade, drawn with Sprite_Current the acting
// member's record: images 1 and 0 at the fading shade 0x80 - +9 (subtractive,
// then additive), images 3 and 2 at +9. +9 is the task's, read at each call.
void DrawCrossFade(const unsigned char* self) {
    using Draw = void (__cdecl*)(unsigned, unsigned, unsigned);
    MH_AT(Draw, bof3::addr::Weretiger_DrawSprite)(1, 2, static_cast<unsigned char>(0x80 - self[9]));
    MH_AT(Draw, bof3::addr::Weretiger_DrawSprite)(0, 1, static_cast<unsigned char>(0x80 - self[9]));
    MH_AT(Draw, bof3::addr::Weretiger_DrawSprite)(3, 2, self[9]);
    MH_AT(Draw, bof3::addr::Weretiger_DrawSprite)(2, 1, self[9]);
}

}  // namespace

// original 0x4B4290: the cross-fade drawn (Sprite_Current the acting member's
// record, then the task back); then the form's DAT file loaded by the owner's
// word +0x2C (0: 0x2EC, else 0x2ED) and its facing +8 (0 or 1: as said; else
// 0x2EF / 0x2F0); +2 on.
S14_EXPORT void __cdecl Weretiger_LoadForm(void) {
    unsigned char* const self = Sc();
    Sprite_Current = PartyRecord(ActorIndex());
    DrawCrossFade(self);
    const unsigned char* const owner = Owner();
    Sprite_Current = self;
    const bool second = Word(owner + 0x2C) != 0;
    const unsigned char facing = owner[8];
    const bool near = facing == 0 || facing == 1;
    MH_CALL(LoadDatFile)(second ? (near ? 0x2ED : 0x2F0) : (near ? 0x2EC : 0x2EF));
    Inc(Sc()[2]);
}

// original 0x4B4350: the cross-fade drawn; +9 up by 2 until it is 0x80; then,
// once the file has loaded (File_LoadDone), with Sprite_Current still the
// member's record: its animation +8 ensured, its tint released, its palette
// from the CLUT source (0x80D380 + +5 x 0x40, index 0), Battle_StatusTint of
// its status word +0x90, Sprite_SetClutStp, bit 0x40 of its +0 cleared; sound
// effect 0x102; +2 on. The task back as Sprite_Current on every way out.
S14_EXPORT void __cdecl Weretiger_FadeIn(void) {
    unsigned char* const self = Sc();
    Sprite_Current = PartyRecord(ActorIndex());
    DrawCrossFade(self);
    const unsigned char shade = self[9];
    if (shade != 0x80) {
        self[9] = static_cast<unsigned char>(shade + 2);
        Sprite_Current = self;
        return;
    }
    if (MH_CALL(File_LoadDone)() == 0) {
        Sprite_Current = self;
        return;
    }
    MH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
    MH_CALL(Sprite_ReleaseTint)(PartyRecord(ActorIndex()));
    MH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(0x80D380 + (static_cast<std::uint32_t>(Sc()[5]) << 6))),
                                0);
    MH_CALL(Battle_StatusTint)(Word(PartyRecord(ActorIndex()) + 0x90));
    MH_CALL(Sprite_SetClutStp)();
    Sc()[0] &= 0xBF;
    MH_CALL(Sound_PlayEffect)(0x102);
    Inc(self[2]);
    Sprite_Current = self;
}

// original 0x4B4470: the acting member's script ticked once (its record as
// Sprite_Current, then the task back); at its end +2 on.
S14_EXPORT void __cdecl Weretiger_WaitScript(void) {
    unsigned char* const self = Sc();
    Sprite_Current = PartyRecord(ActorIndex());
    const unsigned char done = MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    if (done != 0) Inc(self[2]);
}

// original 0x4B44B0: once bit 0 of +0xB is clear (the burst has ended): the
// kind-2 point and the elevation back from the task's +0x34 / +0x38 / +0x3E,
// the map view rebuilt round it; +9 0x40, +0xA 1, +1 2, +2 0.
S14_EXPORT void __cdecl Weretiger_Return(void) {
    {
        const unsigned char* const s = Sc();
        if (s[0xB] & 1) return;
        Field_Kind2X = Long(s + 0x34);
        Field_Kind2Z = Long(s + 0x38);
        MapView_Elevation = S16(s + 0x3E);
    }
    Call0(bof3::addr::Weretiger_ResetMapView);
    Sc()[9] = 0x40;
    Sc()[0xA] = 1;
    Sc()[1] = 2;
    Sc()[2] = 0;
}

// original 0x4B4510: entry 2. While +0xA is set, the view's runs placed and
// +0xA 0; +9 down; when it was 0: the effect's done flag, the acting member's
// dword +0x134 bit 0 set and byte +0x142 0, the target flagged 0x40, the task
// freed.
S14_EXPORT void __cdecl Weretiger_End(void) {
    if (Sc()[0xA] != 0) {
        MH_CALL(MapView_PlaceRuns)();
        Sc()[0xA] = 0;
    }
    {
        unsigned char* const s = Sc();
        const unsigned char was = s[9];
        s[9] = static_cast<unsigned char>(was - 1);
        if (was != 0) return;
    }
    const unsigned actor = ActorIndex();
    Mem(at::kFlags)[0] |= 4;
    unsigned char* const record = PartyRecord(actor);
    SetLong(record + 0x134, static_cast<std::int32_t>(U(Long(record + 0x134)) | 1u));
    record[0x142] = 0;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B4590: the children's kind-1 task (parameter 0x3A). A
// four-entry stack table by +1: WeretigerCopy_Run, WeretigerBurst_Run,
// WeretigerImage_Run, WeretigerDim_Run.
S14_EXPORT void __cdecl WeretigerChild_Task(void) {
    static constexpr std::uint32_t kKinds[4] = {bof3::addr::WeretigerCopy_Run, bof3::addr::WeretigerBurst_Run,
                                                bof3::addr::WeretigerImage_Run, bof3::addr::WeretigerDim_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("WeretigerChild_Task", phase, 4);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4B45D0: the copy. A three-entry stack table by +2:
// WeretigerFx_Next, WeretigerCopy_Step, BattleFx_FreeTask; then while +0 is
// set, Sprite_UpdateScreen.
S14_EXPORT void __cdecl WeretigerCopy_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::WeretigerFx_Next, bof3::addr::WeretigerCopy_Step,
                                                bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("WeretigerCopy_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4B4610: a two-entry stack table by +3: WeretigerCopy_Spawn,
// WeretigerCopy_Tick.
S14_EXPORT void __cdecl WeretigerCopy_Step(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::WeretigerCopy_Spawn, bof3::addr::WeretigerCopy_Tick};
    const unsigned phase = Sc()[3];
    if (phase >= 2) PastTable("WeretigerCopy_Step", phase, 2);
    magic_harness::Phase(kSteps[phase])();
}

namespace {

// One image of WeretigerCopy_Spawn: a child (kind 1, 0x3A) whose first 0x80
// bytes are the acting member's record, +0x80 the task, bit 0x40 of +0 clear,
// +1 2 (WeretigerImage_Run), +2 0, +6 1, +5 0x3A. Returns the child and the
// task as read after the copy.
struct Image {
    unsigned char* child;
    unsigned char* task;
};
Image SpawnImage() {
    const unsigned slot = NewTask(0x3A);
    unsigned char* const child = TaskSlot(slot);
    CopyRecord(child, PartyRecord(ActorIndex()));
    unsigned char* const task = Sc();
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(task)));
    child[0] &= 0xBF;
    child[1] = 2;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x3A;
    return {child, task};
}
// Its animation (the task's facing + `base`), its screen point, bit 0x88 of
// +0x24 and its +0x27.
void PlaceImage(const Image& im, unsigned base, unsigned x, unsigned y, unsigned char b27) {
    im.child[0xB] = static_cast<unsigned char>(im.task[8] + base);
    SetWord(im.child + 0x2E, x);
    SetWord(im.child + 0x30, y);
    im.child[0x24] |= 0x88;
    im.child[0x27] = b27;
}
// With the image as Sprite_Current: its palette from the CLUT source at
// `palette`, Battle_StatusTint of the acting member's status word, the strip's
// STP bits; the task back.
void PaletteImage(const Image& im, std::uint32_t palette, unsigned index) {
    MH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(palette)), index);
    MH_CALL(Battle_StatusTint)(Word(PartyRecord(ActorIndex()) + 0x90));
    MH_CALL(Sprite_SetClutStp)();
    Sprite_Current = im.task;
}

}  // namespace

// original 0x4B4640: the CLUT strip's words 0xFA0..0xFBF (and their source's)
// set: the first 0, the other 31 0xFFFF, the strip dirty; the VRAM rectangle
// (0x340, 0x100, 0xC0, 0x100) cleared; four images of the acting member
// (SpawnImage) at (0x20, 0x40), (0x20, 0x90), (0x80, 0x40), (0x80, 0x90) with
// animations facing + 0x38 / 0x38 / 0x3C / 0x3C, the first and third with
// their own palettes (source 0x80D440 index 0, 0x80D480 index 1); +3 on.
S14_EXPORT void __cdecl WeretigerCopy_Spawn(void) {
    Gfx_ClutStripSource[0xFA0] = 0;
    Gfx_ClutStrip[0xFA0] = 0;
    for (unsigned k = 0xFA1; k < 0xFC0; ++k) {
        Gfx_ClutStripSource[k] = 0xFFFF;
        Gfx_ClutStrip[k] = 0xFFFF;
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Gfx_ClearRect)(0x340, 0x100, 0xC0, 0x100);
    {
        const Image im = SpawnImage();
        Sprite_Current = im.child;
        PlaceImage(im, 0x38, 0x20, 0x40, 0x7B);
        PaletteImage(im, 0x80D440, 0);
    }
    {
        const Image im = SpawnImage();
        PlaceImage(im, 0x38, 0x20, 0x90, 0x7D);
    }
    {
        const Image im = SpawnImage();
        Sprite_Current = im.child;
        PlaceImage(im, 0x3C, 0x80, 0x40, 0x7C);
        PaletteImage(im, 0x80D480, 1);
    }
    const Image im = SpawnImage();
    PlaceImage(im, 0x3C, 0x80, 0x90, 0x7D);
    Inc(im.task[3]);
}

// original 0x4B49D0: the copy's script ticked once (its answer unread); the
// owner's +2 on (the parent's wait at step 7 ends); +2 on (BattleFx_FreeTask
// next), +3 0.
S14_EXPORT void __cdecl WeretigerCopy_Tick(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    Inc(Owner()[2]);
    Inc(Sc()[2]);
    Sc()[3] = 0;
}

// original 0x4B4A00: the burst. A five-entry stack table by +2: _Start,
// _Emit, _Grow, _WaitOwner, _End; then a draw-mode packet (tpage 0x2F) on
// layer 2 and every live streak record (+0 set) run in turn as the pool's
// current one (WeretigerStreak_Run).
S14_EXPORT void __cdecl WeretigerBurst_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::WeretigerBurst_Start, bof3::addr::WeretigerBurst_Emit,
                                                bof3::addr::WeretigerBurst_Grow, bof3::addr::WeretigerBurst_WaitOwner,
                                                bof3::addr::WeretigerBurst_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("WeretigerBurst_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x2F, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const record = PoolRecord(i);
        if (record[0] == 0) continue;
        SetLong(Mem(kPoolCurrent), static_cast<std::int32_t>(Key(record)));
        Call0(bof3::addr::WeretigerStreak_Run);
    }
}

// original 0x4B4A90: +0xB 0, +9 0x40, six streaks allocated, +2 on.
S14_EXPORT void __cdecl WeretigerBurst_Start(void) {
    Sc()[0xB] = 0;
    Sc()[9] = 0x40;
    Call0(bof3::addr::WeretigerStreak_Alloc6);
    Inc(Sc()[2]);
}

// original 0x4B4AC0: six streaks allocated; +9 down; at 0 +2 on.
S14_EXPORT void __cdecl WeretigerBurst_Emit(void) {
    Call0(bof3::addr::WeretigerStreak_Alloc6);
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4B4AF0: six streaks allocated; on even frames +0xB up, and once
// it is 0x18 or more the owner's +2 on (the parent's step 4 ends) and +2 on.
S14_EXPORT void __cdecl WeretigerBurst_Grow(void) {
    Call0(bof3::addr::WeretigerStreak_Alloc6);
    if (Mem(kFrameCounter)[0] & 1) return;
    unsigned char* const s = Sc();
    const unsigned char size = s[0xB];
    if (size >= 0x18) {
        Inc(Owner()[2]);
        Inc(Sc()[2]);
        return;
    }
    s[0xB] = static_cast<unsigned char>(size + 1);
}

// original 0x4B4B30: once the owner has reached step 0xA, +2 on; six streaks
// allocated (a tail jmp).
S14_EXPORT void __cdecl WeretigerBurst_WaitOwner(void) {
    if (Owner()[2] == 0xA) Inc(Sc()[2]);
    Call0(bof3::addr::WeretigerStreak_Alloc6);
}

// original 0x4B4B50: once no streak record is live: bit 0 of the owner's +0xB
// cleared (the parent's _Return waits for it), the task freed.
S14_EXPORT void __cdecl WeretigerBurst_End(void) {
    for (unsigned i = 0; i < kPoolCount; ++i)
        if (PoolRecord(i)[0] != 0) return;
    Owner()[0xB] &= 0xFE;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B4B90: the first six free streak records (+0 0), in order,
// marked live (+0 1); fewer if fewer are free. Nothing else of them is set:
// their +1 is whatever the last use left (0 when a streak ends).
S14_EXPORT void __cdecl WeretigerStreak_Alloc6(void) {
    unsigned short found = 0;
    for (unsigned short i = 0; i < kPoolCount; ++i) {
        unsigned char* const record = PoolRecord(i);
        if (record[0] != 0) continue;
        ++found;
        record[0] = 1;
        if (found == 6) return;
    }
}

// original 0x4B4BD0: the pool's current record run: a two-entry stack table
// by its +1: WeretigerStreak_Start, WeretigerStreak_Draw.
S14_EXPORT void __cdecl WeretigerStreak_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::WeretigerStreak_Start, bof3::addr::WeretigerStreak_Draw};
    const unsigned phase = PoolCurrent()[1];
    if (phase >= 2) PastTable("WeretigerStreak_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B4C00: the current record: its angle word +2 Rand() & 0xFFF;
// both radii +0xC / +0x10 the task's +0xB << 16; their growth 0x32000 and
// 0x19800; its centre the acting member's screen point (+0x2E, +0x30) plus
// the facing's offset pair at 0x65ABF8 (signed bytes, by the member's +8,
// unchecked); +1 on.
S14_EXPORT void __cdecl WeretigerStreak_Start(void) {
    const std::uint32_t r = static_cast<std::uint32_t>(MH_CALL(Rand)());
    SetWord(PoolCurrent() + 2, r & 0xFFF);
    SetLong(PoolCurrent() + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Sc()[0xB]) << 16));
    SetLong(PoolCurrent() + 0x10, static_cast<std::int32_t>(static_cast<std::uint32_t>(Sc()[0xB]) << 16));
    SetLong(PoolCurrent() + 0x14, 0x32000);
    SetLong(PoolCurrent() + 0x18, 0x19800);
    {
        const unsigned char* const record = PartyRecord(ActorIndex());
        const int dx = static_cast<signed char>(Mem(kStreakOffsets + 2u * record[8])[0]);
        SetLong(PoolCurrent() + 4, dx + S16(record + 0x2E));
    }
    {
        const unsigned char* const record = PartyRecord(ActorIndex());
        const int dy = static_cast<signed char>(Mem(kStreakOffsets + 1 + 2u * record[8])[0]);
        SetLong(PoolCurrent() + 8, dy + S16(record + 0x30));
    }
    Inc(PoolCurrent()[1]);
}

namespace {

// A radius's whole part (dword `off` of the current record, sar 16) times a
// sine or cosine, sar 12, plus the centre word `centre`: a 16-bit sum.
unsigned Spoke(int trig, unsigned off, unsigned centre) {
    const unsigned char* const r = PoolCurrent();
    return (static_cast<unsigned>(Mul12(trig, Long(r + off) >> 16)) + Word(r + centre)) & 0xFFFF;
}
int AngleAt(int k) { return S16(PoolCurrent() + 2) + k; }

}  // namespace

// original 0x4B4CE0: the current record grown - both radii +0xC / +0x10 on by
// their growth +0x14 / +0x18, then each growth x 5 / 4 (toward zero) - and
// drawn: five lines at the angles +2 .. +2 + 4, each from the outer radius's
// point to the inner's (WeretigerStreak_DrawLine); once the second radius's
// whole part is past 0x1C0, +0 and +1 0 (the record free). The record is read
// again through 0x684788 after every call, as the original reads it.
S14_EXPORT void __cdecl WeretigerStreak_Draw(void) {
    {
        unsigned char* const r = PoolCurrent();
        SetLong(r + 0xC, static_cast<std::int32_t>(U(Long(r + 0xC)) + U(Long(r + 0x14))));
    }
    {
        unsigned char* const r = PoolCurrent();
        SetLong(r + 0x10, static_cast<std::int32_t>(U(Long(r + 0x10)) + U(Long(r + 0x18))));
    }
    {
        unsigned char* const r = PoolCurrent();
        SetLong(r + 0x14, DivPow2(static_cast<int>(U(Long(r + 0x14)) * 5u), 2));
    }
    {
        unsigned char* const r = PoolCurrent();
        SetLong(r + 0x18, DivPow2(static_cast<int>(U(Long(r + 0x18)) * 5u), 2));
    }
    using Line = void (__cdecl*)(int, int, int, int);
    for (int k = 0; k < 5; ++k) {
        int v = MH_CALL(Math_Sin)(AngleAt(k));
        const unsigned y1 = Spoke(v, 0x10, 8);
        v = MH_CALL(Math_Cos)(AngleAt(k));
        const unsigned x1 = Spoke(v, 0x10, 4);
        v = MH_CALL(Math_Sin)(AngleAt(k));
        const unsigned y0 = Spoke(v, 0xC, 8);
        v = MH_CALL(Math_Cos)(AngleAt(k));
        const unsigned x0 = Spoke(v, 0xC, 4);
        MH_AT(Line, bof3::addr::WeretigerStreak_DrawLine)(static_cast<int>(x0), static_cast<int>(y0),
                                                            static_cast<int>(x1), static_cast<int>(y1));
    }
    unsigned char* const r = PoolCurrent();
    if (static_cast<std::int32_t>(U(Long(r + 0x10)) & 0xFFFF0000u) <= 0x1C00000) return;
    r[0] = 0;
    PoolCurrent()[1] = 0;
}

// original 0x4B50A0: one semi-transparent gouraud line (LINE_G2) from (x0,
// y0) to (x1, y1) - each the low word of its argument, signed - its ends
// shaded (0xF0, 0x14, 0x78) and black, linked on layer 2.
S14_EXPORT void __cdecl WeretigerStreak_DrawLine(int x0, int y0, int x1, int y1) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineG2)(p);
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[4] = 0xF0;
    PutFloat(p + 8, static_cast<short>(x0));
    p[5] = 0x14;
    PutFloat(p + 0xC, static_cast<short>(y0));
    p[6] = 0x78;
    PutFloat(p + 0x18, static_cast<short>(x1));
    PutFloat(p + 0x1C, static_cast<short>(y1));
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    MH_CALL(Gfx_CommitPrim)(2, 0x24);
}

// original 0x4B5120: an image. A three-entry stack table by +2:
// WeretigerImage_Play, WeretigerFx_Next, WeretigerImage_End; then while +0 is
// set, Sprite_UpdateScreen.
S14_EXPORT void __cdecl WeretigerImage_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::WeretigerImage_Play, bof3::addr::WeretigerFx_Next,
                                                bof3::addr::WeretigerImage_End};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("WeretigerImage_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4B5160: animation +0xB ensured, then the script ticked until it
// reports its end, all in one call; +2 on.
S14_EXPORT void __cdecl WeretigerImage_Play(void) {
    MH_CALL(Sprite_EnsureAnimation)(Sc()[0xB]);
    while (MH_CALL(Sprite_ScriptTickOnce)() == 0) {
    }
    Inc(Sc()[2]);
}

// original 0x4B5190: +2 on (a step of the copy's and the image's tables).
S14_EXPORT void __cdecl WeretigerFx_Next(void) { Inc(Sc()[2]); }

// original 0x4B51A0: the image's own tint released, the task freed.
S14_EXPORT void __cdecl WeretigerImage_End(void) {
    MH_CALL(Sprite_ReleaseTint)(Sc());
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4B51C0: the backdrop dim, a jmp through WeretigerDim_Steps (four
// entries: MagicFx_ClearCount9, _Down, _WaitOwner, FxDim_Up) by +2, unchecked.
S14_EXPORT void __cdecl WeretigerDim_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::MagicFx_ClearCount9, bof3::addr::WeretigerDim_Down,
                                                bof3::addr::WeretigerDim_WaitOwner, bof3::addr::FxDim_Up};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("WeretigerDim_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B51E0: +9 down; at 0xFA (-6) +2 on; the backdrop tinted by +9 as
// a signed level (AreaMap_TintClut).
S14_EXPORT void __cdecl WeretigerDim_Down(void) {
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0xFA) Inc(s[2]);
    MH_CALL(AreaMap_TintClut)(static_cast<signed char>(Sc()[9]));
}

// original 0x4B5220: once the owner has reached step 0xA, +2 on.
S14_EXPORT void __cdecl WeretigerDim_WaitOwner(void) {
    if (Owner()[2] == 0xA) Inc(Sc()[2]);
}

// original 0x4B5240: one semi-transparent 0x50 x 0x48 sprite of the form's
// sheet (tpage 0x340 / 0x100, 4-bit, blend `abr`), image `image` (u, v from
// the 4-byte entries at 0x65AC10, unchecked), shaded `shade`, at
// Sprite_Current's screen point (+0x2E - 0x20, or - 0x28 for images 2 and 3;
// +0x30 - 0x40); between a draw-mode packet and the sprite, both linked at
// Sprite_Current's position.
S14_EXPORT void __cdecl Weretiger_DrawSprite(unsigned image, unsigned abr, unsigned shade) {
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(2, abr & 0xFF, 0x340, 0x100);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage & 0xFFFF, 0);
    {
        const unsigned char* const s = Sc();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)),
                                    0, 0xC);
    }
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetSprt)(p);
    p[4] = static_cast<unsigned char>(shade);
    p[5] = static_cast<unsigned char>(shade);
    p[6] = static_cast<unsigned char>(shade);
    const unsigned char which = static_cast<unsigned char>(image);
    PutFloat(p + 8, S16(Sc() + 0x2E) - (which > 1 ? 0x28 : 0x20));
    PutFloat(p + 0xC, S16(Sc() + 0x30) - 0x40);
    p[0x14] = Mem(kImageUV + 4u * which)[0];
    p[0x15] = Mem(kImageUV + 2 + 4u * which)[0];
    SetWord(p + 0x18, 0x50);
    SetWord(p + 0x1A, 0x48);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 0,
                                0x1C);
}

// original 0x4B5350: the field's map view rebuilt round the kind-2 point: the
// two words at 0x905EB6 / 0x905EC6 of every 0x48-byte entry up to 0x929EC6
// cleared; the focus (0x7FFF - x >> 8, 0x8000 - z >> 8), the origin words
// (x >> 16 - 0x18, z >> 16 + 3), the draw table's count 0; the elevation
// offset AreaMap_Word1E (or, when that is 0, -2 x the elevation), brought
// inside -0x1FF..0x1FF by shifting the rows (MapView_ShiftRowsNext /
// _ShiftRowsPrev, 0x200 at a time); the scroll, two move-script words and
// the height scale 0, a redraw of 3; the 56 x 28 cell items' +2 cleared and
// each mapped (MapView_CellToMap); the draw-item pool's free list 0..0x3FF,
// its top 1; column 0x1B, row 0x37; AreaMap_BakePatches; AreaMap_SetupEntries
// (a tail jmp).
S14_EXPORT void __cdecl Weretiger_ResetMapView(void) {
    // The original walks 0x905EC6 up to 0x929EC6, every half of its 1,024
    // items; DIV-0062's pool is read through its pointer and count instead.
    for (unsigned q = 0; q < draw_pool::Count() * 2; ++q) {
        unsigned char* const half = draw_pool::Items() + q * 0x48u;
        SetWord(half + 0x36, 0);
        SetWord(half + 0x46, 0);
    }
    const std::int32_t x = Field_Kind2X;
    const std::int32_t z = Field_Kind2Z;
    MapView_FocusX = 0x7FFF - (x >> 8);
    DrawTable_Count = 0;
    MapView_Origin[0] = static_cast<short>((x >> 16) - 0x18);
    MapView_FocusZ = 0x8000 - (z >> 8);
    MapView_Origin[1] = static_cast<short>((z >> 16) + 3);
    std::uint16_t offset = AreaMap_Word1E;
    if (offset == 0) offset = static_cast<std::uint16_t>((0u - U(MapView_Elevation)) << 1);
    MapView_ElevationOffset = offset;
    MapView_ScrollX = 0;
    if (static_cast<short>(offset) >= 0x200) {
        do {
            MH_CALL(MapView_ShiftRowsNext)();
            offset = static_cast<std::uint16_t>(MapView_ElevationOffset + 0xFE00);
            MapView_ElevationOffset = offset;
        } while (static_cast<short>(offset) >= 0x200);
    }
    if (static_cast<short>(offset) <= -0x200) {
        do {
            MH_CALL(MapView_ShiftRowsPrev)();
            offset = static_cast<std::uint16_t>(MapView_ElevationOffset + 0x200);
            MapView_ElevationOffset = offset;
        } while (static_cast<short>(offset) <= -0x200);
    }
    MoveScript_F3Divisor = 0;
    MoveScript_FAWord = 0;
    MapView_HeightScale = 0;
    MapView_Redraw = 3;
    unsigned char* item = MapView_CellItems;
    for (int row = 0; item < MapView_CellItems + 0x1880; ++row) {
        for (int col = 0; col < 0x1C; ++col, item += 4) {
            SetWord(item + 2, 0);
            MH_CALL(MapView_CellToMap)(row, col, item);
        }
    }
    for (unsigned k = 0; k < draw_pool::Count(); ++k) draw_pool::Free()[k] = static_cast<unsigned short>(k);   // DIV-0062: the pool's size
    DrawItemPool_Top = 1;
    MapView_Column = 0x1B;
    MapView_Row = 0x37;
    MH_CALL(AreaMap_BakePatches)();
    MH_CALL(AreaMap_SetupEntries)();
}

// ===========================================================================
// MAGIC065 (row 70, Pilfer read one id down)

// original 0x4B58F0: an item's name into Text_Records (16 bytes, Str_CopyN)
// by its index (low byte) and category (low byte): 1 a weapon (NameTable_
// Weapons, 0x1C a name), 2 armour (NameTable_Armour, 0x1A), 3 an accessory
// (NameTable_Accessories, 0x18), any other a consumable (NameTable_
// Consumables, 0x16). Answers what Str_CopyN answers.
S14_EXPORT char* __cdecl Item_CopyName(unsigned index, unsigned category) {
    const unsigned i = index & 0xFF;
    const unsigned char* name;
    switch (category & 0xFF) {
    case 1: name = Mem(bof3::addr::NameTable_Weapons) + i * 0x1C; break;
    case 2: name = Mem(bof3::addr::NameTable_Armour) + i * 0x1A; break;
    case 3: name = Mem(bof3::addr::NameTable_Accessories) + i * 0x18; break;
    default: name = Mem(bof3::addr::NameTable_Consumables) + i * 0x16; break;
    }
    return MH_CALL(Str_CopyN)(reinterpret_cast<char*>(Mem(bof3::addr::Text_Records)), reinterpret_cast<const char*>(name), 0x10);
}

// ===========================================================================
// MAGIC066 (row 17, Tsunami read one id down)

// original 0x4B59A0: the kind-2 task. A three-entry stack table by +1:
// Tsunami_Start, Tsunami_Rings, and MAGIC077's Leech_WaitOrbs (at the owner's
// signal +0xB 0xFF: the target flagged 0x40, the done flag, free).
S14_EXPORT void __cdecl Tsunami_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Tsunami_Start, bof3::addr::Tsunami_Rings,
                                                 bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Tsunami_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4B59D0: the owner's position +0x34 / +0x38 / +0x3C copied; the
// wave (kind 1, 0x11, +0x80 this task, +1 0); CLUT row 26 back from its source
// with the STP bit, and row 2's first 16 words the same, the first of them
// then without it; the strip dirty; sound effect 0x100; +0xB 0, +9 0x18, +1
// on.
S14_EXPORT void __cdecl Tsunami_Start(void) {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    const unsigned slot = NewTask(0x11);
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
    child[1] = 0;
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    for (unsigned k = 0x200; k < 0x210; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStrip[0x200] = Gfx_ClutStripSource[0x200];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayEffect)(0x100);
    Sc()[0xB] = 0;
    Sc()[9] = 0x18;
    Inc(Sc()[1]);
}

// original 0x4B5AB0: +9 down; at 0 the rings (kind 1, 0x11, +0x80 this task,
// +1 1), the target flagged 0x20, +1 on.
S14_EXPORT void __cdecl Tsunami_Rings(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned slot = NewTask(0x11);
    const unsigned char target = TargetByte();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
    child[1] = 1;
    MH_CALL(Battle_SetTargetFlags)(target, 0x20);
    Inc(Sc()[1]);
}

// original 0x4B5B10: the children's kind-1 task, a jmp through
// TsunamiChild_Kinds (two entries: the wave, the rings) by +1, unchecked.
S14_EXPORT void __cdecl TsunamiChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::TsunamiWave_Run, bof3::addr::TsunamiRings_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("TsunamiChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4B5B30: a call through TsunamiWave_Steps (five entries) by +2;
// then, unless the owner's +0xB is 0xFF (the wave has ended), the actor
// matrix, the wave, the matrix popped (a tail jmp).
S14_EXPORT void __cdecl TsunamiWave_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::TsunamiWave_Start, bof3::addr::TsunamiWave_Advance,
                                                bof3::addr::TsunamiWave_Grow, bof3::addr::TsunamiWave_Shrink,
                                                bof3::addr::TsunamiWave_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("TsunamiWave_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    if (Owner()[0xB] == 0xFF) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::TsunamiWave_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4B5B60: the wave at the kind-2 point + (-0x12, 0x88) << 16, the
// owner's height; the angle +0xB 0x1A, +9 0x10, the width +0x10 0, the
// amplitude +0x14 0xF0; +2 on.
S14_EXPORT void __cdecl TsunamiWave_Start(void) {
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(U(Field_Kind2X) + 0xFFEE0000u));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(U(Field_Kind2Z) + 0x880000u));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0x1A;
    Sc()[9] = 0x10;
    SetLong(Sc() + 0x10, 0);
    SetLong(Sc() + 0x14, 0xF0);
    Inc(Sc()[2]);
}

// original 0x4B5BD0: z down by 0xC000; +9 down; at 0 +2 on.
S14_EXPORT void __cdecl TsunamiWave_Advance(void) {
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x38, static_cast<std::int32_t>(U(Long(s + 0x38)) + 0xFFFF4000u));
    }
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4B5C00: the angle down (mod 0x20), the width up by 2; at 0x80 +2
// on.
S14_EXPORT void __cdecl TsunamiWave_Grow(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x1F);
    SetLong(s + 0x10, static_cast<std::int32_t>(U(Long(s + 0x10)) + 2));
    if (Long(s + 0x10) == 0x80) Inc(s[2]);
}

// original 0x4B5C30: the angle down (mod 0x20), the amplitude and the width
// down by 2; at a width of 0x20 +2 on.
S14_EXPORT void __cdecl TsunamiWave_Shrink(void) {
    unsigned char* const s = Sc();
    s[0xB] = static_cast<unsigned char>((s[0xB] - 1) & 0x1F);
    SetLong(s + 0x14, static_cast<std::int32_t>(U(Long(s + 0x14)) - 2));
    SetLong(s + 0x10, static_cast<std::int32_t>(U(Long(s + 0x10)) - 2));
    if (Long(s + 0x10) == 0x20) Inc(s[2]);
}

// original 0x4B5C70: the amplitude down by 2, the width by 1; at a width of 0
// the owner's +0xB 0xFF (Leech_WaitOrbs's signal) and the task freed.
S14_EXPORT void __cdecl TsunamiWave_End(void) {
    unsigned char* const s = Sc();
    SetLong(s + 0x14, static_cast<std::int32_t>(U(Long(s + 0x14)) - 2));
    SetLong(s + 0x10, static_cast<std::int32_t>(U(Long(s + 0x10)) - 1));
    if (Long(s + 0x10) != 0) return;
    Owner()[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// TsunamiWave_Draw's edge i: the near edge moves to the far one (x 0 and
// 0x1000, y and z the last edge's), the far edge at y (-0x80 - width / 2) x i
// (width +0x10, sar 1), z the amplitude +0x14 x the sine of ((+0xB + i) & 0x1F)
// << 7, sar 12; then the shade by the step: at step 4 the width / 2 (toward
// zero) x `step4`, else `shade`.
void WaveEdge(int i, unsigned step4, unsigned shade) {
    const std::uint16_t y = VW(2), z = VW(4);
    SetVW(0x12, y);
    SetVW(0x1A, y);
    {
        const unsigned char* const s = Sc();
        SetVW(0x10, 0);
        SetVW(0x14, z);
        SetVW(0x18, 0x1000);
        SetVW(0x1C, z);
        const unsigned angle = ((static_cast<unsigned>(s[0xB]) + static_cast<unsigned>(i)) & 0x1Fu) << 7;
        SetVW(0, 0);
        SetSW(6, angle);
        const int half = Long(s + 0x10) >> 1;
        SetVW(2, static_cast<unsigned>(-0x80 - half) * static_cast<unsigned>(i));
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        const int w = Mul12(v, SS(0));
        const std::uint16_t y2 = VW(2);
        SetVW(8, 0x1000);
        SetVW(4, static_cast<unsigned>(w));
        SetVW(0xC, static_cast<unsigned>(w));
        SetVW(0xA, y2);
    }
    const unsigned char* const s = Sc();
    if (s[2] == 4) SetSW(2, static_cast<unsigned>(DivPow2(Long(s + 0x10), 1)) * step4);
    else SetSW(2, shade);
}

}  // namespace

// original 0x4B5CB0: the wave - 127 semi-transparent textured quads (tpage
// 0x340 / 0x100, CLUT row 0x1FA) up a sine of amplitude +0x14 whose phase is
// (+0xB + i) & 0x1F, each 0x1000 wide, stepping back in y by (0x80 + width / 2)
// a quad; shaded 0x80 (at step 4 width / 2 x 8), the texture's v stepping with
// (+0xB + i) & 7; then, from edge 0x7E again, two gouraud quads (edges 0x7F
// and 0x80) shaded 0xC0 (at step 4 width / 2 x 12), the last bright at its far
// edge, the one before at its near; between two draw-mode packets on layer 2.
S14_EXPORT void __cdecl TsunamiWave_Draw(void) {
    DrawModeCommit(0xB5, 2);
    {
        const unsigned char* const s = Sc();
        SetSW(0, Word(s + 0x14));
        const unsigned angle = (s[0xB] & 0x1Fu) << 7;
        SetVW(0, 0);
        SetSW(6, angle);
        SetVW(2, 0);
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        SetVW(4, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    int i = 1;
    for (; i < 0x80; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        WaveEdge(i, 8, 0x80);
        p[4] = SB(2);
        p[5] = SB(2);
        p[6] = SB(2);
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA));
        SetSW(8, (0xEu - ((static_cast<unsigned>(Sc()[0xB]) + static_cast<unsigned>(i)) & 7u)) << 4);
        p[0x14] = 0;
        p[0x24] = 0xF8;
        p[0x15] = SB(8);
        p[0x25] = SB(8);
        p[0x34] = 0;
        p[0x44] = 0xF8;
        p[0x35] = static_cast<unsigned char>(SB(8) + 0x10);
        p[0x45] = static_cast<unsigned char>(SB(8) + 0x10);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x48);
    }
    i -= 2;
    {
        const unsigned char* const s = Sc();
        SetVW(0, 0);
        const unsigned angle = ((static_cast<unsigned>(s[0xB]) + static_cast<unsigned>(i)) & 0x1Fu) << 7;
        SetSW(6, angle);
        const int half = Long(s + 0x10) >> 1;
        SetVW(2, static_cast<unsigned>(-0x80 - half) * static_cast<unsigned>(i));
        const int v = MH_CALL(Math_Sin)(static_cast<short>(angle));
        ++i;
        SetVW(4, static_cast<unsigned>(Mul12(v, SS(0))));
    }
    for (; i < 0x81; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        WaveEdge(i, 12, 0xC0);
        if (i == 0x80) {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(2);
        } else {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = SB(2);
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        }
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        MH_CALL(Gfx_CommitPrim)(2, 0x44);
    }
    DrawModeCommit(0x95, 2);
}

// original 0x4B6160: the rings, a jmp through TsunamiRings_Steps (three
// entries: _Start, _Grow, _End) by +2, unchecked.
S14_EXPORT void __cdecl TsunamiRings_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::TsunamiRings_Start, bof3::addr::TsunamiRings_Grow,
                                                bof3::addr::TsunamiRings_End};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("TsunamiRings_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4B6180: the four ring phases (0x68478C) 0, +9 0, +2 on.
S14_EXPORT void __cdecl TsunamiRings_Start(void) {
    unsigned char* const s = Sc();
    SetLong(Mem(kRingPhases), 0);
    s[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4B61A0: the rings drawn; +9 up; at 0x60 +2 on.
S14_EXPORT void __cdecl TsunamiRings_Grow(void) {
    Call0(bof3::addr::TsunamiRings_Draw);
    Inc(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] == 0x60) Inc(s[2]);
}

// original 0x4B61D0: the rings drawn; +9 up; at 0x80 the task freed.
S14_EXPORT void __cdecl TsunamiRings_End(void) {
    Call0(bof3::addr::TsunamiRings_Draw);
    Inc(Sc()[9]);
    if (Sc()[9] == 0x80) MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// One ring round a live actor: +0xB the height index, +0xA the actor's index
// on its side, the position its +0x34 / +0x38 / +0x3C plus the height's
// offsets; the ring's matrix, its quads, the matrix popped.
void RingAt(unsigned height, unsigned index, const unsigned char* actor) {
    const unsigned char* const offset = Mem(kRingOffsets + 0xC * height);
    Sc()[0xB] = static_cast<unsigned char>(height);
    Sc()[0xA] = static_cast<unsigned char>(index);
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(U(Long(offset)) + U(Long(actor + 0x34))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(U(Long(actor + 0x38)) + U(Long(offset + 4))));
    SetLong(Sc() + 0x3C, static_cast<std::int32_t>(U(Long(offset + 8)) + U(Long(actor + 0x3C))));
    Call0(bof3::addr::TsunamiRing_PushMatrix);
    Call0(bof3::addr::TsunamiRing_DrawQuads);
    MH_CALL(Gte_PopMatrix)();
}

}  // namespace

// original 0x4B6200: a draw-mode packet (tpage 0x35) on layer 3; for each of
// the three heights (0x65AC54, 12 bytes each) a ring round every enemy 0..7
// and then every party member 0..2 that Battle_ActorIsOut says is not out;
// each ring phase whose index x 3 is below +9 one on (and, at step 1, kept
// below 0x10); a draw-mode packet (tpage 0x15) on layer 3.
S14_EXPORT void __cdecl TsunamiRings_Draw(void) {
    DrawModeCommit(0x35, 3);
    for (unsigned height = 0; height < 3; ++height)
        for (unsigned e = 0; e < 8; ++e) {
            if (MH_CALL(Battle_ActorIsOut)(e + 3) & 0xFF) continue;
            RingAt(height, e, Mem(at::kEnemies + e * at::kEnemyStride));
        }
    for (unsigned height = 0; height < 3; ++height)
        for (unsigned m = 0; m < 3; ++m) {
            if (MH_CALL(Battle_ActorIsOut)(m) & 0xFF) continue;
            RingAt(height, m, PartyRecord(m));
        }
    {
        const unsigned char* const s = Sc();
        for (unsigned j = 0, c = 0; c < 0xC; c += 3, ++j) {
            unsigned char& phase = Mem(kRingPhases + j)[0];
            if (c < s[9]) Inc(phase);
            if (s[2] == 1) phase &= 0xF;
        }
    }
    DrawModeCommit(0x15, 3);
}

// original 0x4B63C0: the ring's quads: for each ring phase j (0..3) whose index
// x 3 is below +9 and whose phase b is below 0x10, one semi-transparent
// textured quad (tpage 0x340 / 0x100, CLUT row 0x1E2) at the angles 0x200,
// 0x600, 0xE00, 0xA00 of radius b x 4 + 0x32, at depth -(b x 15), shaded 0x41
// - (b >> 1) x 4, on layer 3.
S14_EXPORT void __cdecl TsunamiRing_DrawQuads(void) {
    for (unsigned j = 0, c = 0; c < 0xC; c += 3, ++j) {
        if (static_cast<int>(c) >= static_cast<int>(Sc()[9])) continue;
        if (Mem(kRingPhases + j)[0] >= 0x10) continue;
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyFT4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const unsigned b = Mem(kRingPhases + j)[0];
            SetSW(0, b * 4 + 0x32);
            SetSW(4, b * 15);
        }
        static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
        for (unsigned v = 0; v < 4; ++v) {
            int t = MH_CALL(Math_Cos)(kAngles[v]);
            SetVW(8 * v, static_cast<unsigned>(Mul12(t, SS(0))));
            t = MH_CALL(Math_Sin)(kAngles[v]);
            SetVW(8 * v + 2, static_cast<unsigned>(Mul12(t, SS(0))));
            SetVW(8 * v + 4, static_cast<unsigned>(-static_cast<int>(SW(4))));
        }
        SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100));
        SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1E2));
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0x20;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0x20;
        p[0x44] = 0x20;
        p[0x45] = 0x20;
        const unsigned shade = (0x41u - ((static_cast<unsigned>(Mem(kRingPhases + j)[0]) >> 1) << 2)) & 0xFFFF;
        SetSW(2, shade);
        p[4] = static_cast<unsigned char>(shade);
        p[5] = SB(2);
        p[6] = SB(2);
        Rtp4(p);
        MH_CALL(Gte_PrimDepths4_10)(p);
        MH_CALL(Gfx_CommitPrim)(3, 0x48);
    }
}

namespace {

// The ring's matrix block as the original lays it out on its stack: MATRIX at
// +0x14 of the frame, RotTrans writing its translation at the MATRIX's +0x14.
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

// original 0x4B6620: the ring's matrix pushed: Camera_Matrix x the rotation
// (0xC00, 0, 0) - a quarter turn back about x, the ring lying flat - with the
// translation RotTrans of (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2))
// from the task's +0x34 / +0x38 / +0x3E, read after the push.
S14_EXPORT void __cdecl TsunamiRing_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const short rot[4] = {0xC00, 0, 0, 0};
    short v[4];
    {
        const unsigned char* const s = Sc();
        v[0] = static_cast<short>((Long(s + 0x34) >> 9) - 0x4000);
        v[1] = static_cast<short>((Long(s + 0x38) >> 9) - 0x4000);
        v[2] = static_cast<short>(-(S16(s + 0x3E) / 2));
        v[3] = 0;
    }
    Matrix m;
    long flag;
    // The original pushes a third argument (the flag) to Gte_RotTrans, which
    // takes two (cdecl: the caller pops it).
    using RotTransFn = void (__cdecl*)(const short*, long*, long*);
    S14_AS(RotTransFn, Gte_RotTrans)(v, m.t, &flag);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

void MagicS14_Inject() {
    if (bof3::WantsShadow("magic_s14")) magic_s14::SelfTest();
    BOF3_INJECT(Weretiger_Task);
    BOF3_INJECT(Weretiger_Run);
    BOF3_INJECT(Weretiger_Pose);
    BOF3_INJECT(Weretiger_WaitPose);
    BOF3_INJECT(Weretiger_FocusActor);
    BOF3_INJECT(Weretiger_Burst);
    BOF3_INJECT(Weretiger_Copy);
    BOF3_INJECT(Weretiger_Release);
    BOF3_INJECT(Weretiger_LoadForm);
    BOF3_INJECT(Weretiger_FadeIn);
    BOF3_INJECT(Weretiger_WaitScript);
    BOF3_INJECT(Weretiger_Return);
    BOF3_INJECT(Weretiger_End);
    BOF3_INJECT(WeretigerChild_Task);
    BOF3_INJECT(WeretigerCopy_Run);
    BOF3_INJECT(WeretigerCopy_Step);
    BOF3_INJECT(WeretigerCopy_Spawn);
    BOF3_INJECT(WeretigerCopy_Tick);
    BOF3_INJECT(WeretigerBurst_Run);
    BOF3_INJECT(WeretigerBurst_Start);
    BOF3_INJECT(WeretigerBurst_Emit);
    BOF3_INJECT(WeretigerBurst_Grow);
    BOF3_INJECT(WeretigerBurst_WaitOwner);
    BOF3_INJECT(WeretigerBurst_End);
    BOF3_INJECT(WeretigerStreak_Alloc6);
    BOF3_INJECT(WeretigerStreak_Run);
    BOF3_INJECT(WeretigerStreak_Start);
    BOF3_INJECT(WeretigerStreak_Draw);
    BOF3_INJECT(WeretigerStreak_DrawLine);
    BOF3_INJECT(WeretigerImage_Run);
    BOF3_INJECT(WeretigerImage_Play);
    BOF3_INJECT(WeretigerFx_Next);
    BOF3_INJECT(WeretigerImage_End);
    BOF3_INJECT(WeretigerDim_Run);
    BOF3_INJECT(WeretigerDim_Down);
    BOF3_INJECT(WeretigerDim_WaitOwner);
    BOF3_INJECT(Weretiger_DrawSprite);
    BOF3_INJECT(Weretiger_ResetMapView);
    BOF3_INJECT(Item_CopyName);
    BOF3_INJECT(Tsunami_Task);
    BOF3_INJECT(Tsunami_Start);
    BOF3_INJECT(Tsunami_Rings);
    BOF3_INJECT(TsunamiChild_Task);
    BOF3_INJECT(TsunamiWave_Run);
    BOF3_INJECT(TsunamiWave_Start);
    BOF3_INJECT(TsunamiWave_Advance);
    BOF3_INJECT(TsunamiWave_Grow);
    BOF3_INJECT(TsunamiWave_Shrink);
    BOF3_INJECT(TsunamiWave_End);
    BOF3_INJECT(TsunamiWave_Draw);
    BOF3_INJECT(TsunamiRings_Run);
    BOF3_INJECT(TsunamiRings_Start);
    BOF3_INJECT(TsunamiRings_Grow);
    BOF3_INJECT(TsunamiRings_End);
    BOF3_INJECT(TsunamiRings_Draw);
    BOF3_INJECT(TsunamiRing_DrawQuads);
    BOF3_INJECT(TsunamiRing_PushMatrix);
}
