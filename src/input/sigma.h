// σ: the Config's six lines as one permutation of the PlayStation's six face
// and shoulder bits (docs/button-sigma.md; the design is
// docs/button-roles-by-release.md 7 with the owner's answers of 2026-10-17,
// its section 8, staged in docs/button-unification-round.md 2 "SG").
//
// The game's code is Japan's on every install: every hard-coded test and every
// save word is in Japan's bits. So the table is kept in Japan's terms:
//
//   Sigma(b)    for Japan's button b, the button the line whose default it is
//               in Japan's release (circle Speak, square Move, cross Action,
//               triangle Menu, R1 View, L1 Change) is on now. A hard-coded
//               test of b asks Sigma(b); an icon drawn from the PC's own text
//               or art (Japan's buttons) is drawn as Sigma(b).
//   SigmaFrom(R, b)
//               for release R's button b (an icon from R's text or art):
//               Sigma(M_R^-1(b)), M_R the release's map from Japan's buttons
//               (identity for JP, PSP-JP and the PC; P for the PlayStation
//               West; S for PSP-EU). With the Config at R's own defaults it is
//               the identity: the round doc's "σ read against the install's
//               release defaults".
//
// The table is rebuilt at the three writers of the Config words (the
// rebind ConfigScreen_Controller 0x4613B0's 0x461459..0x46153B, LoadMenu_Apply
// 0x5885D0's 0x58860E.., Boot_Task 0x496B60's copy ending 0x496C25; all three
// ours), which call ConfigWritten after their own stores. Two divergences ride
// on it, each its own switch (PatchBytes on a flag of ours, after every
// module's self-test):
//
//   ButtonSigma          (DIV-0133) the table kept; words 7 and 8
//                        (Field_ConfirmButtons, Field_CancelButtons) written
//                        as the Speak line's word and the Action line's word,
//                        so menus confirm on Speak's button and cancel on
//                        Action's; six words that are not a permutation of
//                        the six bits refused with a Fatal (rule 4: nothing
//                        guessed).
//   StartConfigBySource  (DIV-0134) the starting Config - Boot_Task's
//                        nine words and LoadMenu_Apply's Start-held reset -
//                        the install's release's, by the cache manifest's
//                        sources: JP first (the PC's own data counts as JP,
//                        the owner's confirmation of 2026-10-17), then the
//                        PlayStation West, then the PSP.
//
// Under BOF3X_ORIGINAL=ButtonSigma, Sigma and SigmaFrom are the identity:
// the original tests Japan's bit itself, whatever the Config says.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bof3x::input::sigma {

// The releases whose defaults differ (button-roles-by-release.md 2).
enum class Release : unsigned char {
    kJapan,    // SLPS_009.90, PSP-JP's BOOT.BIN, the PC: Japan's assignment
    kWest,     // US, EU, FR, DE: P of Japan's
    kPspEu,    // PSP-EU's BOOT.BIN: S of Japan's
};
const char* ReleaseName(Release r);

// The six bits σ permutes, in the Config's line order (Speak, Move, Action,
// Menu, View, Change are words 0, 1, 6, 2, 3, 4 - the line-to-slot table
// 0x6536F0).
constexpr int kLines = 6;
constexpr std::uint16_t kSixBits = 0xFC;              // L1 R1 triangle circle cross square
constexpr std::uint16_t kL2R2 = 0x03;                 // ORed into words 0 and 7, never a line's button
extern const unsigned char kLineWord[kLines];         // {0, 1, 6, 2, 3, 4}
extern const char* const kLineName[kLines];           // "Speak", "Move", ...

// M_R on one bit: Japan's button -> release R's button for the same line.
// Bits outside the six pass through unchanged (start, select, the d-pad, L2,
// R2 are the same on every release).
std::uint16_t ReleaseMap(Release r, std::uint16_t bit);
std::uint16_t ReleaseMapInverse(Release r, std::uint16_t bit);

// The release's nine pad words (0x903580..0x903591) as its boot executable
// copies them: Japan's rule (exe_own.cpp FieldButtonDefaults, equal to
// BOF3.exe's 0x656AEC) with each line's button mapped by M_R. With
// `reset`, LoadMenu_Apply's Start-held reset instead: the same buttons with
// no L2 / R2 in words 0 and 7, as the PC's reset 0x588629..0x588670 writes.
void DefaultWords(Release r, bool reset, std::uint16_t out[9]);

// The install's release from a manifest's [[source]] builds (fixtures.toml
// ids): JP if any JP source (pc-zh, psx-jp), else the PlayStation West if any
// of psx-us / psx-eu-en / psx-fr / psx-de, else psp-jp, else psp-eu. An id it
// does not know, or no source at all, is refused: `error` set, kJapan
// returned (the caller Fatals on the error).
Release ReleaseOfSources(const std::vector<std::string>& sources, std::string& error);

// One σ, as a pure value: built from nine words, or refused.
struct Table {
    bool valid = false;
    std::uint16_t now[16] = {};    // by bit index: Japan's button -> the button now (the six only)
    std::uint16_t back[16] = {};   // the inverse
};
// The six lines' words (words 0's L2 / R2 set aside) must each be exactly one
// of the six bits, all different. Otherwise `why` names the first fault and
// the table is not valid.
Table Build(const std::uint16_t words[9], std::string& why);
// σ over a mask: each of the six bits mapped, the rest as they are. Only for
// a valid table.
std::uint16_t Apply(const Table& t, std::uint16_t mask);
std::uint16_t ApplyInverse(const Table& t, std::uint16_t mask);

// --- the live table (the DLL) ---

// Sigma / SigmaInverse take one bit or a mask (each of the six bits mapped,
// the others passed through). Asked while ButtonSigma is on and before any
// writer has built the table: a Fatal.
std::uint16_t Sigma(std::uint16_t bits);
std::uint16_t SigmaInverse(std::uint16_t bits);
std::uint16_t SigmaFrom(Release r, std::uint16_t bits);

// The install's release, as Arm read it (Japan before Arm).
Release InstallRelease();

// Boot_Task, after its copy of 0x656AEC's eighteen bytes; LoadMenu_Apply,
// after its copy from the save block and its Start-held reset (`reset`: the
// reset ran); the rebind, after its swap. Each: the starting words by source
// (StartConfigBySource; the boot and the reset only), then words 7 / 8 and the
// table (ButtonSigma). Nothing at all before Arm, so every module's self-test
// compares Capcom's stores.
void BootDefaultsWritten();
void LoadApplied(bool reset);
void Rebound();

// After every module's self-test: the self-test (BOF3X_SHADOW=sigma), the
// release read, the two switches.
void Arm();

}  // namespace bof3x::input::sigma
