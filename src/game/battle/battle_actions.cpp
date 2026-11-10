// The action phases: the battle's phase 3 (0x42F220, the fourth entry of
// Battle_PhaseDispatch's table) and the seven step tables under it. An
// action goes: the next actor out of the turn order (Battle_BeginAction), its
// command kind (0x904B35) dispatched - kinds 0 and 2, kind 1, the ability
// (kind 4) and the item (kind 5) each with their checks, and kind 3 through
// BattleAction_Kind3Dispatch 0x42F5E0 (round twelve BE1's) - then the effect steps, the steps after it
// (the enemy messages among them), and the end, where a set 0x40 in the round
// flags swaps the actor and the target and runs one more action. What the
// kinds are in game terms is not read here. docs/battle_actions.md.
//
// Every call goes through battle_actions::g (battle_actions_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. The step tables are read in .data, afresh at
// each dispatch, as the originals read them. Everything here is a faithful
// replacement, but for DIV-0179's opt-in banner colour (UnlearnedBlue).
#include "game/battle_actions.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_actions_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/psp_code.h"
#include "game/sound.h"
#include "game/sound_st.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_actions {

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace battle_actions

using namespace battle_actions;

// DIVERGENCE DIV-0179, the unlearned skill in blue (BOF3X_UNLEARNED_BLUE=1,
// unlearned_blue= in bof3x.ini; off by default). The banner pool 0x93B8E0, 8
// entries of 0xC: +0xA is the text colour BattleWin_BannerRun 0x597C70 hands
// Text_DrawAt, which FillBanner always sets to 0. Colour 3 is the sky-blue
// row of FIRST.DAT's text CLUT strip (kind 0, tag 0x8000).
namespace battle_actions { bool g_unlearned_blue = false; }  // ===========================================================================
// Phase 3 and the step dispatches
// ===========================================================================

// ===========================================================================
// Step 0: the next actor
// ===========================================================================

// ===========================================================================
// Step 1: the command kinds
// ===========================================================================

// ===========================================================================
// Kind 4: the ability
// ===========================================================================

// ===========================================================================
// Kind 5: the item
// ===========================================================================

// ===========================================================================
// Step 2: the effects
// ===========================================================================

// original 0x42FD20 (PSX 0x801D3E60): for kind 4, the ability row of the id
// 0x904B80 (by the whole u16), for kind 5 the item row of the record's u16
// +2 (battle_flow.md's ItemRow); when that row's magic file is not 0xFFFF,
// LoadDatFile(0xD0). Any kind: 0x904AA2 counts one.
//
// As the original has it: the ability row is indexed by the whole word
// (Magic_LoadForAbility uses its low byte); neither index is checked.
//
// DIVERGENCE DIV-0167: the PlayStation first keys off voices 16..23
// (`SsUtKeyOffV` 0x8016F10C for s0 = 0x10..0x17, JP 0x801D3E68..0x801D3E84;
// US 0x801D3E88.., the same words but the call's target), cutting what the
// previous action's effect still sounds; the PC has no such step. With the
// switch (PatchBytes("EffectKeyOff") in BattleActions_Inject, after the
// self-test), the same point stops those voices' channels
// (sound::KeyOffEffectVoices, game/sound.h).
//
// DIVERGENCE DIV-0174: the same key-off reaching a one-shot - the buffer each
// of those channels last played without a loop, remembered DLL-side by
// Sound_PlayEffect - under its own switch (PatchBytes("OneShotKeyOff"), after
// the self-test as above; sound_st::KeyOffOneShots, game/sound_st.h), so the
// loops' stop and the one-shots' bisect apart.
namespace battle_actions { std::uint8_t g_effect_key_off = 0; }

// ===========================================================================
// Step 3: after the effects
// ===========================================================================

// ===========================================================================
// Step 4: the action's end
// ===========================================================================

// ===========================================================================

void BattleActions_Inject() {
    if (bof3::WantsShadow("battle_actions")) battle_actions::SelfTest();
    {
        // DIV-0167: after the self-test, which compared Capcom's preload.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("EffectKeyOff", Low32(&battle_actions::g_effect_key_off), &was, &is, 1);
        bof3::Log("DIV-0167    a battle action's effect stops the sound-effect channels the last one left (on unless the line above says OFF)");
    }
    sound_st::ArmOneShotKeyOff();  // DIV-0174: after the self-test, which compared Capcom's preload
    // DIV-0179: BOF3X_UNLEARNED_BLUE = 0 (the original's white, the default)
    // or 1; read after the self-test, which compared the banner with it off,
    // and after the injects, so that IsEnabled sees the check ours.
    {
        char v[8] = "0";
        const DWORD n = GetEnvironmentVariableA("BOF3X_UNLEARNED_BLUE", v, sizeof v);
        if (n >= sizeof v || (std::strcmp(v, "0") != 0 && std::strcmp(v, "1") != 0))
            bof3::Fatal("BOF3X_UNLEARNED_BLUE is \"%s\": 0 or 1", n >= sizeof v ? "(too long)" : v);
        battle_actions::g_unlearned_blue = v[0] == '1';
        if (battle_actions::g_unlearned_blue && !bof3::IsEnabled(bof3::addr::BattleAction_AbilityCheck))
            bof3::Fatal("BOF3X_UNLEARNED_BLUE=1 needs BattleAction_AbilityCheck ours, and it is disabled");
        if (battle_actions::g_unlearned_blue) bof3::Log("DIV-0179    an enemy's skill the party can still learn has its name in blue");
    }
}
