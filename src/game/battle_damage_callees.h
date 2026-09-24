// Internal to battle_damage.cpp and battle_damage_fuzz.cpp: the addresses the
// damage, effect, affinity and turn-order functions touch that symbols.toml
// does not name, and every call they make - through pointers, so that the
// start-up fuzz can stand recording functions in for them, for the originals'
// copies and for ours alike. Callees this module implements are called through
// the same pointers, so each function is tested alone. The calls into other
// groups' functions (0x4456C0, 0x44A650, 0x44F4B0) and into unnamed ones are
// raw addresses here and nowhere else (docs/takeover-queue-round7.md, "The
// rule for calls across groups"). docs/battle_damage.md.
#pragma once

#include <cstdint>

namespace battle_damage {

namespace at {

// The party: ObjTrio 0x802D40, three objects 0x14C apart, the working record
// (PartyWorkingRecords 0x802D50) at +0x10 of each. Offsets below are the
// working record's, so the object's first byte is -0x10.
constexpr std::uint32_t kParty = 0x802D50;
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr int kPPresent = -0x10;     // u8, bit 0: in the battle
constexpr int kPCureId = -0x0B;      // u8, what 0x44F4B0 is handed when a status is cured
constexpr unsigned kPLevel = 0x7A;   // u8
constexpr unsigned kPStatus = 0x80;  // u16 status bits (PSX C+0x0C = 0x80145F0C)
constexpr unsigned kPWeapon = 0x82;  // u8 weapon id (PSX 0x80145F0E)
constexpr unsigned kPHp = 0x88, kPAp = 0x8A;        // u16
constexpr unsigned kPSurvive = 0x8C;                // u8, the overkill below which a kill leaves the flag
constexpr unsigned kPMaxHp = 0x90, kPMaxAp = 0x92;  // u16
constexpr unsigned kPDef = 0x96, kPAgi = 0x98;      // u16
constexpr unsigned kPElements = 0x9F;               // five u8 affinity classes (fire .. wind)
constexpr unsigned kPHoly = 0xA4;                   // u8 holy affinity class
constexpr unsigned kPCommand = 0x115;               // u8 the chosen command (4 an ability, 5 an item)
constexpr unsigned kPCommandId = 0x116;             // u16 its ability id
constexpr unsigned kPShown = 0x11C;                 // u8, status-shown bits (0x11 on a hit, |4 on a full heal)
constexpr unsigned kPShown2 = 0x11D;                // u8, cleared with a cured status
constexpr unsigned kPFlags = 0x120;                 // u32
constexpr unsigned kPFlags2 = 0x124;                // u32
constexpr unsigned kPCharge = 0x134, kPCharge2 = 0x135;   // u8 charge counts (attack, ability)
constexpr unsigned kPWeaponKind = 0x138;            // u8, 5: the weapon's element rolls apply

// The enemies: EnemyWorkingRecords 0x93B9E0, 0x128 apart; the "object" the AI
// and the tests address is the record - 0x80 (0x93B960), so an enemy's
// present byte is the previous record's +0xA8.
constexpr std::uint32_t kEnemy = 0x93B9E0;
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr int kEObject = -0x80;
constexpr int kEPresent = -0x80;     // u8, bit 0
constexpr int kECureId = -0x7B;      // u8
constexpr int kEFamily = 0x0D;       // u8: 4 and 1 double three weapons' damage each (Battle_CalcDamage)
constexpr unsigned kEStatus = 0x12;  // u16
constexpr unsigned kELevel = 0x18;   // u8 (also read as a u16: the enemy attack bonus)
constexpr unsigned kEHp = 0x24, kEAp = 0x26, kEMaxHp = 0x30, kEMaxAp = 0x32, kEAgi = 0x38;   // u16
constexpr unsigned kEElements = 0x3F, kEHoly = 0x44;                                          // u8
constexpr unsigned kECommand = 0x85;
constexpr unsigned kEShown = 0x8C, kEShown2 = 0x8D;
constexpr unsigned kEFlags = 0x90, kEFlags2 = 0x94;   // u32
constexpr unsigned kECharge = 0xA4, kECharge2 = 0xA5;
constexpr unsigned kEScript = 0x70;   // u8, object +0xF0: which AI script
constexpr unsigned kERowsDone = 0x71; // u8, object +0xF1: a bit per script row fired

// The damage scratch: the two actors' stats copied out (PSX 0x801EC23C..).
constexpr std::uint32_t kTargetDef = 0x939F86;     // u16 (PSX 0x801EC25E)
constexpr std::uint32_t kTargetInt = 0x939F8A;     // u16 (PSX 0x801EC262)
constexpr std::uint32_t kAttackerAtk = 0x939FE4;   // u16, read as a dword & 0xFFFF (PSX 0x801EC27C)
constexpr std::uint32_t kAttackerInt = 0x939FEA;   // u16, the charge multiplies it (PSX 0x801EC282)
constexpr std::uint32_t kTurnWork = 0x939A80;      // 11 x (s16 value, u16 actor), the order before sorting

// The battle state (PSX 0x801462E4.., the same order).
constexpr std::uint32_t kBattleFlags = 0x904AA8;   // u8, bit 7 (PSX 0x801462E4)
constexpr std::uint32_t kPartyCount = 0x904AB0;    // u8 (PSX 0x801462EC)
constexpr std::uint32_t kAiByteB1 = 0x904AB1;      // u8, AI opcodes 0x1D / 0x1E
constexpr std::uint32_t kAiByteB2 = 0x904AB2;      // u8, AI opcodes 0xF / 0x1F
constexpr std::uint32_t kAiByteB3 = 0x904AB3;      // u8, AI opcodes 0xF / 0x10 / 0x1F / 0x20
constexpr std::uint32_t kEntryOrder = 0x904AB6;    // 3 actor bytes, 0xFF-ended (PSX 0x801462F2)
constexpr std::uint32_t kEntryCount = 0x904AC3;    // u8 (PSX 0x801462FF)
constexpr std::uint32_t kTurnOrder = 0x904ACC;     // 11 actor bytes (PSX 0x80146308)
constexpr std::uint32_t kTurnCursor = 0x904AE2;    // u8 (PSX 0x8014631E)
constexpr std::uint32_t kTurnCount = 0x904AE3;     // u8 (PSX 0x8014631F)
constexpr std::uint32_t kSideOut = 0x904AE4;       // u8: 1 the enemies may not act, 2 the party, 3 neither
constexpr std::uint32_t kActing = 0x904B34;        // u8 the acting actor, then u8 its kind (4 an ability)
constexpr std::uint32_t kItemCommand = 0x904B40;   // pointer: +2 a u16 (category << 8 | item)
constexpr std::uint32_t kTarget = 0x904B54;        // u8 the target of the result (PSX 0x80146390)
constexpr std::uint32_t kResult = 0x904B60;        // pointer: the result record, +4 HP and +6 AP delta
constexpr std::uint32_t kGate = 0x904B7A;          // u8: when set, an actor takes part only with flag 0x8000
constexpr std::uint32_t kAbility = 0x904B80;       // u16 the ability id (PSX 0x801463BC)
constexpr std::uint32_t kGate2 = 0x904B8E;         // u8: when set, an actor acts only with flag 0x10 (PSX 0x801463CA)
constexpr std::uint32_t kTurnCounter = 0x904B90;   // u32 (PSX 0x801463CC)
constexpr std::uint32_t kAiByteB97 = 0x904B97;     // u8, AI opcode 0x27
constexpr std::uint32_t kCuredBits = 0x904B9A;     // u16, |0x40 / |0x20 when a hit cures a status
constexpr std::uint32_t kPartyCut = 0x904060;      // u8, 2: a quarter off damage to the party (PSX 0x80144F54)
constexpr std::uint32_t kInstantKill = 0x8031F3;   // u8, set with the flag-0x100 kill (PSX 0x801483BF)
constexpr std::uint32_t kKillMessage = 0x669E04;   // u32 handed to 0x44A650 (PSX &0x801EAFF8)

// The AI scripts: 0x8C bytes a script, 16-byte rows, byte 0 the condition.
constexpr std::uint32_t kAiScripts = 0x8C5600;
constexpr std::uint32_t kAiScriptStride = 0x8C;

// Tables in .data (read, never written).
constexpr std::uint32_t kTurnPercent = 0x64E328;    // s16 x 6 by level class (the party's ability bonus)
constexpr std::uint32_t kTurnJitter = 0x64E334;     // s8 x 16 per level class (the enemies' order)
constexpr std::uint32_t kLevelRows = 0x64E374;      // u8 x 6 per row: 0 the enemies', 1 the party's
constexpr std::uint32_t kBonusDivisor = 0x64E380;   // u8, by an index that is always 0 (Battle_BaseDamage)
constexpr std::uint32_t kSkillHandler = 0x64E540;   // u8 per ability id: the handler
constexpr std::uint32_t kItemHandlers = 0x64E72C;   // 4 pointers to u8 per item id, by category
constexpr std::uint32_t kHandlers = 0x64E73C;       // 130 effect handlers
constexpr unsigned kHandlerCount = 130;
constexpr std::uint32_t kPowerVariance = 0x64E94C;  // u16 x 8: 85..120 percent (Effect_SkillDamage)
constexpr std::uint32_t kElementTable = 0x64E95C;   // s16 per affinity class (Battle_ElementAffinity)
constexpr std::uint32_t kHealTable = 0x64E99C;      // s16 per holy class (Effect_HealAmount)
constexpr std::uint32_t kWeaponElement = 0x13;      // NameTable_Weapons record +0x13
constexpr unsigned kWeaponStride = 0x1C;
constexpr unsigned kAbilityStride = 0x18;           // NameTable_Abilities: +1 order kind, +3 heal power, +4 element mask

}  // namespace at

struct Callees {
    // ours, in this module
    unsigned char (__cdecl* can_act)(unsigned actor);                            // Battle_ActorCanAct
    unsigned char (__cdecl* can_command)(unsigned actor);                        // Battle_ActorCanCommand
    unsigned char (__cdecl* level_class)(unsigned level, unsigned row);          // Battle_LevelClass
    short (__cdecl* calc_damage)(unsigned attacker, unsigned target, unsigned element);   // Battle_CalcDamage
    int (__cdecl* base_damage)(unsigned attacker, unsigned target, unsigned mode);        // Battle_BaseDamage
    int (__cdecl* scale_damage)(unsigned attacker, unsigned target, int base);            // Battle_ScaleDamage
    int (__cdecl* element_affinity)(unsigned target, unsigned mask);                      // Battle_ElementAffinity (callers take the low word)
    unsigned char (__cdecl* row_done)(const unsigned char* enemy, unsigned row);          // EnemyAI_RowDone
    // Capcom's: other groups' and unnamed
    unsigned char (__cdecl* is_out)(unsigned actor);                             // 0x4456C0 (group BB)
    void (__cdecl* show_message)(unsigned, unsigned, unsigned, unsigned, unsigned);   // 0x44A650 (group BF)
    unsigned char (__cdecl* element_resisted)(unsigned attacker, unsigned target);    // 0x44FA70
    void (__cdecl* inflict)(unsigned target, unsigned status);                  // 0x44F1D0
    void (__cdecl* cure)(unsigned id, unsigned status);                         // 0x44F4B0 (group BA)
    int (__cdecl* hit_party)(int amount, unsigned attacker, unsigned target);   // 0x446110
    int (__cdecl* hit_enemy)(int amount, unsigned attacker, unsigned target);   // 0x4461B0
    int (__cdecl* party_def)();                                                 // 0x4463E0
    int (__cdecl* psi_affinity)(unsigned target, unsigned mask);                // 0x44F030 (Battle_PsiStatusDeathAffinity; the low word)
    int (__cdecl* rand)();                                                      // Rand, the CRT's
    void (__cdecl* ai_apply)(unsigned char* enemy, const unsigned char* row);   // 0x44B3A0
    void (__cdecl* ai_set_done)(unsigned char* enemy, unsigned row, unsigned on);   // 0x44B2E0
    unsigned char (__cdecl* ai_rows_left)(unsigned row, unsigned enemy);       // 0x44B240
    void (__cdecl* ai_finish)();                                                // 0x44B920
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_damage: the start-up fuzz, battle_damage_fuzz.cpp. Clones
// every original before BattleDamage_Inject patches it.
void SelfTest();

}  // namespace battle_damage
