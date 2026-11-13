#pragma once

// The solo-character mode's party-set supplement (docs/solo-character-party-set.md;
// DIVERGENCE DIV-0228 SoloCharacter). Group PS's module.
//
// A fight whose code dresses a sprite as a character id from the loaded party
// set (fight 16: Boss16_Event's code 2 and BossNina_HookHit dress enemy 2 as id
// 1 by Field_MemberSprite(1, 1)) needs a row holding that id beside the
// champion. When no row of the game's 19 does (id 4 beside id 1), the battle
// keeps the champion's own row and this module puts the id's battle section -
// read from the player's own battle file of a row that holds it, through
// LoadDatFile's whole precedence - into a column of the loaded battle file that
// the battle of one leaves unused, in the arena, and dresses the sprite from
// that column. Nothing is shipped or written to disk: the plan and the
// section's bytes are held in this module's memory for the battle they were
// made for (Game_BattleCount and the plan's inputs) and made again from the
// files for another. The party-set row table 0x669750 is changed only for the
// length of one Field_MemberSprite call.
//
// With SoloCharacter off nothing here reads or writes game state: Arm is asked
// only by SoloBattle_Arm after Solo_ChampionFights(), and Dress and Around act
// only in a battle SoloBattle_ChampionFightsThis() answers for (which asks
// Solo_ChampionFights() first).

namespace solo {

// SoloBattle_Arm's question when ChooseRow found no row holding the champion
// `champion` and `need` (solo_battle.cpp's kNeedsIdInRow): can the battle be
// fought in `base` - ChooseRow's answer for the champion alone (-1 keep the
// current set, else a row) - with `need`'s battle section supplied beside it?
// `event_flags` is the step's 0x904AE5. Answers `base` when it can (and logs
// the plan), -2 when it cannot (logged with the reason; the caller stands
// down as before). Reads the two battle files and holds the plan for the
// battle; writes no game state.
int PartySetExtra_Arm(unsigned char need, unsigned char champion, int base, unsigned char event_flags);

// In place of Field_MemberSprite(member, slot) where a fight's code dresses
// Sprite_Current as `member` from the loaded set. False (nothing done) unless
// the battle is the mode's (SoloBattle_ChampionFightsThis: the champion alone,
// from the game's bytes, so a quick-load answers the same) and the loaded row
// lacks `member`; then the plan for the live state (the set byte, 0x904AAC,
// the champion) - Arm's when held for this battle, else made again from the
// files once - the loaded set data tested against it, the section copied into
// its column and the sprite dressed by Field_MemberSprite
// with that column's byte reading `member` for the call: true. A plan that no
// longer holds (the loaded arena is not the row's battle file, a file gone) is
// a Fatal - the original call would read off the row.
bool PartySetExtra_Dress(unsigned member, unsigned slot);

// Around a call that joins `member` to the party from the battle's set
// (Boss16_Exit's Scenario_CallA(1): ScenaCall_Join1 -> Party_Join(1) ->
// Field_MemberSprite(1, n), measured on champion Rei's run). Unless the
// battle is the mode's (as Dress asks) and the loaded row lacks `member`, just
// `call(arg)`. Else the
// row's free column (the one Dress would take) reads `member` for the call:
// the joined record is drawn from that column's section of whatever set file
// is loaded - a valid section, where the game's call would read off the row.
// The record is the one the exit hides at once (Boss16_Exit's ObjTrio[1]
// |= 0x40) and step 7's Field_PartyLoad(0) rebuilds it from the field's own
// row (solo_battle.h), so nothing of it is ever drawn.
void PartySetExtra_Around(unsigned member, void (*call)(unsigned), unsigned arg);

// The pure logic's self-test (column choice, section bounds, fit, the gate,
// the held plan's key); the count
// of checks. Called from Solo_SelfTest. A Fatal on a case that does not hold.
int Solo_PartySetSelfTest();

}  // namespace solo
