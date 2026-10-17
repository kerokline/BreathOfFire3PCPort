// The .data bytes no disc carries that are the port's own (exe_own.h;
// docs/exe-engine-own.md). Every region is defined by what it means - the
// file names the recipes already list, the strings the port's Windows layer
// passes, initial values, two computed tables, the default field buttons
// by name - never as a copy of the exe's bytes (CLAUDE.md rule 1). Each is
// verified against BOF3.exe's .data at every start (ExeOwn_CheckAtStart).
#include "hook/exe_own.h"

#include <cmath>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "hook/detour.h"
#include "hook/exe_host.h"
#include "hook/exe_image.h"
#include "hook/exe_layout.h"
#include "hook/log.h"
#include "input/bindings.h"

namespace bof3 {
namespace {

#include "hook/exe_own_names.inc"

constexpr std::uint32_t kDataVa = exe_layout::kDataVa;
constexpr std::uint32_t kDataBytes = exe_layout::kDataBytes;

// The compiler's layout of the port's string literals, measured on all of
// these regions (docs/exe-engine-own.md section 2.1): each literal at a
// 4-aligned address, its NUL and zero padding to the next.
constexpr std::uint32_t Slot(std::size_t len) { return static_cast<std::uint32_t>((len + 1 + 3) & ~std::size_t{3}); }

// Strings laid upward from `at`, in order.
void Strings(std::uint8_t* at, std::uint32_t size, std::initializer_list<const char*> list) {
    std::uint32_t o = 0;
    for (const char* s : list) {
        const std::size_t n = std::strlen(s);
        if (o + n + 1 > size) Fatal("exe_own: a string list overruns its region (\"%s\")", s);
        std::memcpy(at + o, s, n + 1);
        o += Slot(n);
    }
}

// A name table's literals as the port's compiler laid them: in the table's
// order, each one below the last, from the region's top down; a name the
// table repeats is laid once (the SND tables repeat five). The region must
// end exactly where the first literal starts.
void Pool(std::uint8_t* at, std::uint32_t size, std::initializer_list<std::pair<const char* const*, std::size_t>> tables) {
    std::uint32_t top = size;
    std::vector<const char*> laid;
    for (const auto& t : tables)
        for (std::size_t i = 0; i < t.second; ++i) {
            const char* s = t.first[i];
            bool seen = false;
            for (const char* l : laid) seen = seen || std::strcmp(l, s) == 0;
            if (seen) continue;
            laid.push_back(s);
            const std::size_t n = std::strlen(s);
            if (Slot(n) > top) Fatal("exe_own: a name pool overruns its region at \"%s\"", s);
            top -= Slot(n);
            std::memcpy(at + top, s, n + 1);
        }
    if (top != 0) Fatal("exe_own: a name pool leaves %u bytes of its region below its last name", (unsigned)top);
}

void Words(std::uint8_t* at, std::uint32_t size, std::initializer_list<std::uint32_t> words) {
    if (words.size() * 4 != size) Fatal("exe_own: %u words for a region of %u bytes", (unsigned)words.size(), (unsigned)size);
    for (std::uint32_t w : words) {
        std::memcpy(at, &w, 4);
        at += 4;
    }
}

void FieldButtonDefaults(std::uint8_t* at, std::uint32_t size) {
    // The field's button map Boot_Task copies to 0x903580 (nine pad words;
    // docs/controls.md section 2.1): the PlayStation button of each action,
    // the Japanese release's assignment (circle speaks and confirms, cross
    // acts and cancels), which the Config screen's Init writes for its six
    // lines too. Speak and confirm carry L2 and R2 as well, as the Config
    // screen's rebinding ORs them back in. Then a zero padding halfword.
    using namespace bof3x::input;
    constexpr std::uint16_t kSpeak = kCircle | kL2 | kR2;   // word 0
    constexpr std::uint16_t kMove = kSquare;                // word 1
    constexpr std::uint16_t kMenu = kTriangle;              // word 2, Field_MenuButton
    constexpr std::uint16_t kView = kR1;                    // word 3
    constexpr std::uint16_t kChange = kL1;                  // word 4
    constexpr std::uint16_t kWord5 = kSelect;               // word 5: no line of the Config screen, no reader alone
    constexpr std::uint16_t kAction = kCross;               // word 6
    constexpr std::uint16_t kConfirm = kCircle | kL2 | kR2; // word 7, Field_ConfirmButtons
    constexpr std::uint16_t kCancel = kCross;               // word 8, Field_CancelButtons
    const std::uint16_t words[10] = {kSpeak, kMove, kMenu, kView, kChange, kWord5, kAction, kConfirm, kCancel, 0};
    if (size != sizeof words) Fatal("exe_own: the field button map's region is %u bytes", (unsigned)size);
    std::memcpy(at, words, sizeof words);
}

void DatNames(std::uint8_t* at, std::uint32_t size) {
    Pool(at, size, {{kDatNames, sizeof kDatNames / sizeof *kDatNames}});
}

void DatFormats(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"DAT\\%s", "BISLPS??.DAT"});   // File_Open's DAT directory; Save_ListFiles' pattern
}

void CfgInitial(std::uint8_t* at, std::uint32_t size) {
    Words(at, size, {1, 1, 1});   // Cfg_Fullscreen, Cfg_RenderMode, Cursor_Hidden: each initialised 1
}

void WinMainMessages(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"Save OK", "Frame Rate = %d", "LOGOS.AVI"});
}

void WindowNames(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"Bof3", "BOF3_ICON"});   // the window class; the icon resource Game_WinMain loads
}

void WinMainFiles(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"CAPCOM.AVI", "BOF3.EXE", "%d %d", "BOF3.CFG", "rt"});   // ... Cfg_Load's format, file, mode
}

// The game's own draw formats and marks, read in place by Capcom's code. Each
// PSX disc keeps its copy inside the reading overlay, at no place the byte map
// can find (each too short to place by search), so a disc-built image left them
// zero: an empty format, nothing drawn (docs/install-gap-census.md 4.1; D310).
void Area75Format(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"%02d.%02dM"});   // Area75_DrawCounters' two counters
}

void Area104Format(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"%02d.%d%d"});    // Area104_Kind6ACountdown's digits
}

void BoardFormats(std::uint8_t* at, std::uint32_t size) {
    // CommuBoard_DrawRows: the dash, a row's three numbers, and the two formats
    // of no number a row without one draws.
    Strings(at, size, {"-", "%d%d%d", "?", "???"});
}

void AbilityFormat(std::uint8_t* at, std::uint32_t size) {
    // BattleResultWin_DrawLevelUp's learned-ability names, EffectKind5E_DrawMenu's
    // title and item name (the Western discs carry none; Japan's and the PSP's do).
    Strings(at, size, {"%s"});
}

void IdentifyMarks(std::uint8_t* at, std::uint32_t size) {
    // Identify_Draw*'s unknown marks: one `?`, then a row of eight drawn by count
    // (no terminator; D278).
    static const char kMarks[] = {'?', 0, 0, 0, '?', '?', '?', '?', '?', '?', '?', '?'};
    if (size != sizeof kMarks) Fatal("exe_own: the Identify marks' region is %u bytes", (unsigned)size);
    std::memcpy(at, kMarks, sizeof kMarks);
}

void PlayTimeSeparator(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"."});   // Menu_DrawPlayTime's, drawn twice in the tiny font as the colon
}

void SlotNumber(std::uint8_t* at, std::uint32_t size) {
    // Menu_DrawSaveSlot's slot number, drawn on every slot of the save and load
    // lists (the port's sixteen files, BISLPS00..0F, numbered in hex). No disc
    // carries it: without it the draw shows the buffer's stale text (D254).
    Strings(at, size, {"%X"});
}

void SaveName(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"BISLPS%02X.DAT"});   // FieldSave_Write's and Save_QuickWrite's file name
}

void NoTrack(std::uint8_t* at, std::uint32_t size) {
    Words(at, size, {0xFFFFFFFFu});   // Music_LoadedTrack: no track's file loaded yet
}

void SndNames(std::uint8_t* at, std::uint32_t size) {
    Pool(at, size, {{kSndKind0, sizeof kSndKind0 / sizeof *kSndKind0},
                    {kSndKind1, sizeof kSndKind1 / sizeof *kSndKind1},
                    {kSndKind2, sizeof kSndKind2 / sizeof *kSndKind2}});
}

void SoundFormats(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"SND\\%s.DAT", "BGM\\%03dN.DAT", "BGM\\%03d.DAT"});   // Sound_LoadStream; Music_LoadFile's two
}

void FmvCommands(std::uint8_t* at, std::uint32_t size) {   // Fmv_Play's and Fmv_WndProc's MCI strings
    Strings(at, size, {"stop vfw wait", "play vfw window from 0 notify", "put vfw destination at 0 0 640 480",
                       "window vfw handle %d", "close vfw wait", "open avivideo!%s%s alias vfw",
                       "open avivideo!%s alias vfw", "pause vfw", "resume vfw"});
}

void ScreenRect(std::uint8_t* at, std::uint32_t size) {
    // Gfx_ScreenRect {0, 0, 640, 480}; then the dword Display_Setup compares
    // with -1 before it stores a mode there (-1: none yet).
    Words(at, size, {0, 0, 640, 480, 0xFFFFFFFFu});
}

void AlphaOpCache(std::uint8_t* at, std::uint32_t size) {
    Words(at, size, {1});   // D3d_AlphaOpCache: stage 0's ALPHAOP taken as MODULATE
}

void DisplayStrings(std::uint8_t* at, std::uint32_t size) {
    // Display_ErrorBox's messages (by its pointer table at 0x66B6AC), the
    // display mode name, the box's caption, the decoder's handle format,
    // File_CdRootBuf's initial root, File_Open's path format and modes.
    Strings(at, size,
            {"DirectSound SetFormat Error!", "DirectSound CreateSoundPrimaryBuffer Error!",
             "DirectSound SetCooperativeLevel Error!", "DirectSound Initial Error or SoundCard Not Found!",
             "Create D3D Light Error!", "Create D3D Material Error!", "Set D3D Viewport3 Error!",
             "Add D3D Viewport3 Error!", "Create D3D Viewport3 Error!", "Setup Texture Format Error!",
             "Create D3D Device Error!", "Create ZBuffer Error!", "Create Clipper Error!", "Create BackSurface Error!",
             "Create PrimarySurface Error!", "SetCooperativeLevel to normal Error!", "GetAttachedSurface Error!",
             "CreateSurface flipping surface Error!", "SetCooperativeLevel to fullscreen Error!",
             "Direct3D3 Device Error!", "Direct3D3 QueryInterface Error!", "DirectDraw4 QueryInterface Error!",
             "DirectDraw Initial Error!", "Software Render", "ERROR", "%X@%X", "C:\\", "%s%s", "rb", "wb"});
}

void GpuGteConstants(std::uint8_t* at, std::uint32_t size) {
    // Gpu_OtTerminator: an ordering table's end mark, all ones. Then
    // Gte_IdentityRotation: the 3 x 3 identity in the GTE's fixed point (ONE
    // = 0x1000 on the diagonal) and a zero padding halfword.
    Words(at, 4, {0xFFFFFFFFu});
    const std::int16_t one = 0x1000;
    const std::int16_t m[10] = {one, 0, 0, 0, one, 0, 0, 0, one, 0};
    if (size != 4 + sizeof m) Fatal("exe_own: the GPU / GTE constants' region is %u bytes", (unsigned)size);
    std::memcpy(at + 4, m, sizeof m);
}

void SinTable(std::uint8_t* at, std::uint32_t size) {
    // Math_SinTable: the quarter wave of PSX libgte's 4096-step circle,
    // entry i = 4096 sin(2 pi i / 4096) rounded to nearest, i = 0..1024.
    if (size != 1025 * 2) Fatal("exe_own: the sine table's region is %u bytes", (unsigned)size);
    for (int i = 0; i <= 1024; ++i) {
        const auto v = static_cast<std::uint16_t>(std::floor(4096.0 * std::sin(i * 3.14159265358979323846 / 2048.0) + 0.5));
        std::memcpy(at + 2 * i, &v, 2);
    }
}

void PadName(std::uint8_t* at, std::uint32_t size) {
    Strings(at, size, {"Microsoft SideWinder game pad"});   // DInput_EnumJoystick's product name, compared case-blind
}

struct Row {
    ExeOwnRegion region;
    void (*fill)(std::uint8_t* at, std::uint32_t size);
};

// clang-format off
// regions:
constexpr Row kRows[] = {
    {{0x60ABF0, 0x60ABFC, "Area75_DrawCounters' counter format (D310)"}, Area75Format},
    {{0x61BC74, 0x61BC80, "Area104_Kind6ACountdown's format (D310)"}, Area104Format},
    {{0x64FFE4, 0x652894, "Dat_FileNames' 742 names (recipes/pc-zh.toml)"}, DatNames},
    {{0x652894, 0x6528A9, "the DAT directory format and the save files' pattern"}, DatFormats},
    {{0x653084, 0x653098, "CommuBoard_DrawRows' dash and formats (D310)"}, BoardFormats},
    {{0x654900, 0x654904, "the level-up's ability name format (D310)"}, AbilityFormat},
    {{0x656AEC, 0x656B00, "the field button map's defaults Boot_Task copies (a rule)"}, FieldButtonDefaults},
    {{0x65AAC4, 0x65AAD0, "the Identify panel's unknown marks (D278)"}, IdentifyMarks},
    {{0x65DA44, 0x65DA50, "Cfg_Fullscreen, Cfg_RenderMode, Cursor_Hidden: 1"}, CfgInitial},
    {{0x65DA54, 0x65DA76, "WinMain's messages and the logo AVI's name"}, WinMainMessages},
    {{0x65DA84, 0x65DA96, "the window class and the icon's name"}, WindowNames},
    {{0x65DAB8, 0x65DAE7, "the Capcom AVI, the exe's name, BOF3.CFG and its format and mode"}, WinMainFiles},
    {{0x6637E0, 0x6637E4, "Menu_DrawPlayTime's separator (D310)"}, PlayTimeSeparator},
    {{0x6639D4, 0x6639D8, "the save slot's number format (D254)"}, SlotNumber},
    {{0x664068, 0x664077, "the save file's name format"}, SaveName},
    {{0x6653BC, 0x6653C0, "Music_LoadedTrack: none (-1)"}, NoTrack},
    {{0x6653C0, 0x666F9C, "Sound_LoadStream's 891 SND/ names (recipes/pc-zh.snd.toml)"}, SndNames},
    {{0x666F9C, 0x666FC5, "the SND/ and BGM/ file formats"}, SoundFormats},
    {{0x66B5D8, 0x66B6A7, "the AVI player's MCI commands"}, FmvCommands},
    {{0x66B708, 0x66B71C, "Gfx_ScreenRect and the display mode's none (-1)"}, ScreenRect},
    {{0x66B720, 0x66B724, "D3d_AlphaOpCache: 1"}, AlphaOpCache},
    {{0x66B928, 0x66BC3F, "the DirectX error messages, the caption, the file layer's formats"}, DisplayStrings},
    {{0x66BC40, 0x66BC58, "Gpu_OtTerminator and Gte_IdentityRotation"}, GpuGteConstants},
    {{0x66BC58, 0x66C45A, "Math_SinTable (a rule)"}, SinTable},
    {{0x66C7B0, 0x66C7CE, "the pad looked for by product name"}, PadName},
};
// end regions
// clang-format on

constexpr std::size_t kCount = sizeof kRows / sizeof kRows[0];

consteval bool WellFormed() {
    std::uint32_t end = kDataVa;
    for (const Row& r : kRows) {
        if (r.region.lo < end || r.region.hi <= r.region.lo || r.region.hi > kDataVa + kDataBytes) return false;
        end = r.region.hi;
    }
    return true;
}
static_assert(WellFormed(), "regions sorted, not overlapping, non-empty, inside .data");

ExeOwnRegion g_regions[kCount];

std::uint32_t HeldBytes() {
    std::uint32_t n = 0;
    for (const Row& r : kRows) n += r.region.hi - r.region.lo;
    return n;
}

}  // namespace

const ExeOwnRegion* ExeOwn_Regions(std::size_t* count) {
    for (std::size_t i = 0; i < kCount; ++i) g_regions[i] = kRows[i].region;
    *count = kCount;
    return g_regions;
}

void ExeOwn_Fill(std::uint8_t* data_base) {
    for (const Row& r : kRows) {
        std::uint8_t* at = data_base + (r.region.lo - kDataVa);
        const std::uint32_t size = r.region.hi - r.region.lo;
        std::memset(at, 0, size);
        r.fill(at, size);
    }
}

void ExeOwn_Verify(const std::uint8_t* pc_data, const char* against) {
    std::vector<std::uint8_t> scratch(pc_data, pc_data + kDataBytes);
    ExeOwn_Fill(scratch.data());
    for (const Row& r : kRows) {
        const std::uint32_t o = r.region.lo - kDataVa, size = r.region.hi - r.region.lo;
        if (std::memcmp(scratch.data() + o, pc_data + o, size) == 0) continue;
        std::uint32_t i = 0;
        while (scratch[o + i] == pc_data[o + i]) ++i;
        Fatal("exe_own: 0x%06X..0x%06X (%s): the engine's bytes are not %s's, the first difference at 0x%06X "
              "(ours 0x%02X, theirs 0x%02X)", (unsigned)r.region.lo, (unsigned)r.region.hi, r.region.what, against,
              (unsigned)(r.region.lo + i), (unsigned)scratch[o + i], (unsigned)pc_data[o + i]);
    }
    Log("exe_own: %u regions, %u bytes of .data held by the engine (%s .. %s), each equal to %s",
        (unsigned)kCount, (unsigned)HeldBytes(), kRows[0].region.what, kRows[kCount - 1].region.what, against);
}

void ExeOwn_CheckAtStart() {
    const auto* live = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(kDataVa));
    // A disc-built image mapped (the disc-only round, IN): the running .data
    // holds the fill itself, so the regions are verified against the file's
    // .data when the file is there, and cannot be without it.
    if (ExeImage_DiscMapped()) {
        if (!ExeFile_Present()) {
            Log("exe_own: %u regions, %u bytes filled over a disc-built image; no BOF3.exe to verify them against",
                (unsigned)kCount, (unsigned)HeldBytes());
            return;
        }
        std::vector<std::uint8_t> file(kDataBytes);
        ExeFile_Read(kDataVa, file.data(), kDataBytes);
        ExeOwn_Verify(file.data(), "BOF3.exe's .data (the file; the running .data is a disc-built image)");
        return;
    }
    ExeOwn_Verify(live, "BOF3.exe's .data");
    if (!WantsShadow("exe_own")) return;
    // The self-test: into a zeroed image the fill writes the regions' bytes,
    // equal to the PC's, and leaves every other byte zero.
    std::vector<std::uint8_t> zero(kDataBytes, 0);
    ExeOwn_Fill(zero.data());
    std::size_t k = 0;
    std::uint32_t nonzero = 0;
    for (std::uint32_t o = 0; o < kDataBytes; ++o) {
        const std::uint32_t a = kDataVa + o;
        while (k < kCount && a >= kRows[k].region.hi) ++k;
        const bool inside = k < kCount && a >= kRows[k].region.lo;
        if (!inside && zero[o]) Fatal("exe_own self-test: the fill wrote 0x%06X, outside every region", (unsigned)a);
        if (inside && zero[o] != live[o])
            Fatal("exe_own self-test: 0x%06X filled into a zero image is 0x%02X, BOF3.exe's 0x%02X", (unsigned)a,
                  (unsigned)zero[o], (unsigned)live[o]);
        nonzero += zero[o] != 0;
    }
    Log("exe_own self-test: a fill into a zeroed image writes %u non-zero bytes in %u regions, each the PC's, nothing "
        "outside them", (unsigned)nonzero, (unsigned)kCount);
}

}  // namespace bof3
