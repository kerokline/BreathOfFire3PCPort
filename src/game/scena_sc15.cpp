// Scenario chapters 15..19 (the PSX's SCENA15..SCENA19.EMI), compiled into the
// exe at 0x567DC0..0x56B2A0 with 0x537580 (chapter 15), 0x56C080 (chapter 16's
// object hook, the one of its slots the title-states work left) and
// 0x56C130..0x56D5E0 (chapters 17, 18 and 19). docs/scena_sc15.md.
//
//   - Chapter 15, vtable Scena15_Hooks: the frame jumps through Scena15_States
//     on the s8 0x8034E2 (0 is chapter 13/14's 0x5646B0, state 1; 1
//     Scena15_EnterArea, the set-up of the area entered, state 2; 2
//     Scena15_Run, which jumps through Scena15_Runs on MoveScript_Var7 to six
//     runs). Each run is a step machine on the u8 0x8034E5 (a switch in .text,
//     bounded) that waits on the script counters 0x903848.., the message
//     request, the wait word, a file or an effect, calls two or three engine
//     functions and sets the next step. The object hook calls
//     Scena15_Objects[object +0x86] (object, row); the step and arrive hooks
//     answer in al.
//   - Chapter 16's object hook calls Scena16_Objects[object +0x86].
//   - Chapter 17 (the staff roll): the frame's states 0 and 1 set up area 0xC7;
//     state 2 jumps through Scena17_Parts on the s8 0x8034E3 to three parts,
//     each a table of steps on MoveScript_Var7 (Scena17_Part1Steps,
//     Part2Steps, Part3Steps). The parts draw: a fade, a disc of rays, a
//     letterbox, the scrolling roll of names from Scena17_RollLines in a
//     font of Scena17_GlyphWidths. The last step restarts the task at
//     Scena17_EndTask, which runs Scena17_EndSteps on Game_Step: the story
//     flags and a return to chapter 15's area 0x8F, then a restart to Boot_Task.
//   - Chapters 18 and 19: a frame, a run table of one entry, an object table of
//     one bare ret; 19's one run places an effect on a key.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies, and every
// cell is read where and when the original reads it. The .data tables are
// read in place and unchecked as the original reads them; where the original
// would jump to what is not code, ours aborts (CodeAt). No divergence: each
// function is a faithful replacement; the latent defects are described in the
// doc, section 7.
#include "game/scena_sc15.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round ten's cleanup, docs/round-10-cleanup.md item 1):
// every constant here whose target is ours reads bof3::addr::<Name>. The values
// are unchanged - the fuzz keys on them - and the comments' "nobody owns" is as
// of the wave that wrote them.
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc15_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

#define SC15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using namespace scena_sc15;
using std::int32_t;
using std::uint32_t;
using VoidFn = void (__cdecl*)();
using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);

unsigned char* AtScenaSc15(uint32_t a) { return move_script::At(a); }
unsigned char& B(uint32_t a) { return AtScenaSc15(a)[0]; }
std::uint16_t W(uint32_t a) { return move_script::Word(AtScenaSc15(a)); }
void SetW(uint32_t a, unsigned v) { move_script::SetWord(AtScenaSc15(a), v); }
uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(AtScenaSc15(a))); }
uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }
void SetL(uint32_t a, uint32_t v) { move_script::SetLong(AtScenaSc15(a), static_cast<int32_t>(v)); }
void SetL(unsigned char* p, uint32_t v) { move_script::SetLong(p, static_cast<int32_t>(v)); }
std::uint16_t W(const unsigned char* p) { return move_script::Word(p); }
void SetW(unsigned char* p, unsigned v) { move_script::SetWord(p, v); }

// kByteFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
// The script counters (MoveScript_CounterOps) and the chapters' other cells.
constexpr uint32_t kCounter0 = 0x903848;
constexpr uint32_t kCounter1 = 0x903849;
constexpr uint32_t kCounter3 = 0x90384B;
// kFlagRow: now Chapter_FlagRow (symbols.toml). the flag bits Flags_Test / Flags_Set are given
// kStoryRow: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, the story flags
// kRow14: now Cond_Flags[0x70] (symbols.toml). Cond_Flags + 0x70
// kRow3: now Cond_Flags[0x78] (symbols.toml). Cond_Flags + 0x78 (chapter 15's row is 0x904008)
// kWait: now Transition_Busy (symbols.toml). Transition_Busy
// kRequest: now Field_Request (symbols.toml). Field_Request
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber
// kPass: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects, 20 records of 0x80
// kSprites: now Sprite_Objects (symbols.toml). Sprite_Objects, 30 records of 0xA4
// kDistance: now Camera_Distance (symbols.toml). Camera_Distance (s16)
// kShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY
// kAngle0: now Camera_Angles (symbols.toml). Camera_Angles[0]
// kAngle1: now Camera_Angles[1] (symbols.toml). Camera_Angles[1]
constexpr uint32_t kAngle2 = 0x929ECC;      // Cond_AngleFB's word
constexpr uint32_t kInputPressed = 0x7E1BEC;   // Input_Pressed
// kF3Divisor: now MoveScript_F3Divisor (symbols.toml). MoveScript_F3Divisor
// kFAWord: now MoveScript_FAWord (symbols.toml). MoveScript_FAWord
// kPacketNext: now Gfx_PacketNext (symbols.toml). Gfx_PacketNext
// kClutStrip: now Gfx_ClutStrip (symbols.toml). Gfx_ClutStrip: 32 rows of 0x100 colours
// kClutDirty: now Gfx_ClutStripDirty (symbols.toml). Gfx_ClutStripDirty
// kGameStep: now Game_Step (symbols.toml). Game_Step
// kVertices: now Prim_VertexScratch (symbols.toml). Prim_VertexScratch

// kRollScroll: now Scena15_Bytes[6] (symbols.toml). the roll's scroll within a line, 0..0x15
// kRollSkip: now Scena15_Bytes[7] (symbols.toml). the roll skipped (Input_Pressed 0x20)

// kEndSteps: now Scena17_EndSteps (symbols.toml). Scena17_EndSteps, 3
// Chapters 18's and 19's.
// kStates18: now Scena18_States (symbols.toml). Scena18_States, 3
// kRuns18: now Scena18_Runs (symbols.toml). Scena18_Runs, 1
// kObjects18: now Scena18_Objects (symbols.toml). Scena18_Objects, 1
// kStates19: now Scena19_States (symbols.toml). Scena19_States, 3
// kRuns19: now Scena19_Runs (symbols.toml). Scena19_Runs, 1
// kObjects19: now Scena19_Objects (symbols.toml). Scena19_Objects, 1

constexpr ::bof3::image::CodeVa kBootTask{bof3::addr::Boot_Task};    // Boot_Task's address, as the original pushes it

// A handler out of one of the chapters' .data tables, read in place and
// unchecked as the original reads it; where the original would jump to what
// is not code (an index past its table), ours aborts. During the fuzz the
// tables hold the harness's recorders, outside .text.
uint32_t CodeAt(uint32_t table, int index, const char* who) {
    const uint32_t at = table + 4u * static_cast<uint32_t>(index);
    const uint32_t entry = L(at);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %d reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)at, (unsigned)::bof3::VaOf(at));
    return entry;
}
VoidFn Handler(uint32_t table, int index, const char* who) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(CodeAt(table, index, who)));
}
// An object hook's handler, called (object, the flag row) as 0x56D6D0's slot 1
// calls through a chapter's object table.
void CallObject(uint32_t table, unsigned char* object, const char* who) {
    unsigned char* const row = AtScenaSc15(Chapter_FlagRow);
    reinterpret_cast<ObjectFn>(static_cast<std::uintptr_t>(CodeAt(table, object[0x86], who)))(object, row);
}

void SetAt(uint32_t row, unsigned n) { SH_CALL(Flags_Set)(AtScenaSc15(row), n); }
void ClearAt(uint32_t row, unsigned n) { SH_CALL(Flags_Clear)(AtScenaSc15(row), n); }

unsigned char* Effect(unsigned slot) { return AtScenaSc15(::bof3::Img(Effect_Objects_addr) + ((slot & 0xFFu) << 7)); }

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::EffectObject>(p) is given effect
// objects (Effect_Objects).

unsigned Area() { return Game_AreaNumber; }
void Step(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }

bool LoadDone() { return SH_CALL(File_LoadDone)() != 0; }
void Msg(unsigned id) { SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id)); }
void ChangeArea(unsigned area, int32_t x, int32_t z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void Transition(unsigned k) { SH_CALL(Transition_Start)(static_cast<unsigned char>(k)); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Set40() { SH_CALL(ScriptFlags_Set40)(); }

// Effect_FindFree; with a slot, its record live (+0 = 1) and of `kind` (+5).
unsigned char NewEffect(unsigned char kind) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = kind;
    }
    return s;
}

// DIVERGENCE DIV-0148: the staff roll of the language played. A layer built
// from a US or PAL disc carries its release's roll as kind 17 (FIRST.DAT,
// tools/exe_roll.py): the lines and text, which .data cannot hold (its pointer
// words are the PC's, Scena17_RollLines included). Taken when the chunk is
// walked (ScenaSc15_RollApply): the lines as addresses into our copy of the
// text, the -1 after them, then the 15 entries the draw reads past it
// (Scena17_RollLines' 367 hold the PC's 351, its -1 and 15 more): the
// release's 14 - the end card is the fifth - and a 0 for the 15th, the
// section's next word, drawn below the screen. Empty without the chunk (the
// original text, a Japanese or PSP layer): .data's table, the Japanese roll.
constexpr char kRollMagic[8] = {'B', 'O', 'F', '3', 'X', 'R', 'L', '2'};
constexpr uint32_t kRollMinLines = 100, kRollMaxLines = 1024, kRollMaxText = 0x4000, kRollAfter = 14, kRollAhead = 15;
std::vector<unsigned char> g_roll_text;
}  namespace scena_sc15 { std::vector<uint32_t> g_roll_lines; }
namespace {
void LoadRoll(unsigned tag, const unsigned char* payload, unsigned size) {
    if (tag != 1) bof3::Fatal("DIV-0148: a staff-roll chunk of tag %u (only 1 is a roll; rebuild the language)", tag);
    const std::vector<unsigned char> raw(payload, payload + size);
    uint32_t n = 0, t = 0;
    if (raw.size() < 16 || std::memcmp(raw.data(), kRollMagic, 8) != 0)
        bof3::Fatal("DIV-0148: the staff-roll chunk is not a roll (rebuild the language)");
    std::memcpy(&n, raw.data() + 8, 4);
    std::memcpy(&t, raw.data() + 12, 4);
    const uint32_t words = n + kRollAfter;
    if (n < kRollMinLines || n > kRollMaxLines || t > kRollMaxText || raw.size() != 16u + 4u * words + t)
        bof3::Fatal("DIV-0148: the staff-roll chunk holds %u lines and %u text bytes in %u bytes", n, t, (unsigned)raw.size());
    g_roll_text.assign(raw.begin() + 16 + 4 * words, raw.end());
    scena_sc15::g_roll_lines.assign(n + 1 + kRollAhead, 0);
    for (uint32_t i = 0; i < words; ++i) {
        uint32_t w;
        std::memcpy(&w, raw.data() + 16 + 4 * i, 4);
        if (w == 0) continue;
        if (w > t || std::memchr(g_roll_text.data() + (w - 1), 0, t - (w - 1)) == nullptr)
            bof3::Fatal("DIV-0148: the staff-roll chunk's word %u starts at text offset %u with no NUL after it", i, w - 1);
        scena_sc15::g_roll_lines[i < n ? i : i + 1] =
            Low32(g_roll_text.data() + (w - 1));
    }
    scena_sc15::g_roll_lines[n] = 0xFFFFFFFFu;
    bof3::Log("DIV-0148: the language's staff roll, %u lines (%u text bytes)", n, t);
}

}  // namespace

// ===========================================================================
// Chapter 15

// ===========================================================================
// Chapter 16's object hook

// ===========================================================================
// Chapter 17: the staff roll

namespace {

// DIVERGENCE DIV-0207 (the owner, 2026-11-07): the ending's disc stops growing
// at kFinRadius and the darkness outside it stays at level 0xFF - the camp
// scenes' night - under the Fin panel, instead of Capcom's growth to 1,200
// and the level lightened to 0x80 over step 6. Draw-only: the steps, their
// counts and the script are the original's. Armed after the self-test
// (PatchBytes "FinNight"); BOF3X_ORIGINAL=FinNight (or BOF3X_OFF) gives the
// original's disc back.
}  namespace scena_sc15 { unsigned char g_fin_night = 0; }  namespace {
constexpr unsigned kFinRadius = 1000;

}  // namespace

// ===========================================================================
// Chapters 18 and 19

void ScenaSc15_RollApply(unsigned tag, const unsigned char* payload, unsigned size) { LoadRoll(tag, payload, size); }

void ScenaSc15_Inject() {
    if (bof3::WantsShadow("scena_sc15")) scena_sc15::SelfTest();
    {   // DIV-0207: armed after the self-test, which compared Capcom's disc.
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FinNight", Low32(&scena_sc15::g_fin_night), &was, &is, 1);
        bof3::Log("DIV-0207    the ending's disc held at %u, the night kept under the Fin panel: %s", kFinRadius,
                  scena_sc15::g_fin_night ? "on" : "OFF (BOF3X_ORIGINAL=FinNight)");
    }
}
