// Internal to battle_turn_steps.cpp and battle_turn_steps_fuzz.cpp: the battle
// globals the per-turn steps touch, and every call they make - through
// pointers, so that the start-up fuzz can stand recording functions in for
// them, for ours and for the originals' copies alike. docs/battle_turn_steps.md.
//
// Three kinds of transfer are not calls to a named function:
//   - the eight dispatch stubs `jmp [table + 4 * state]` through the .data
//     tables named in symbols.toml (BattleRoundEnd_Steps .. BattleResult_ExpSteps);
//     ours reads the same table, and the fuzz swaps the table entries for
//     recorders;
//   - two steps switch on a state through a jump table of their own
//     (0x4303B4, 4 entries; 0x4305FC, 14 entries), relocated in the copies;
//   - three hooks called through dwords of the battle globals: 0x904B6C (with
//     2, answering in al), 0x904B64 and 0x904B68 (no argument).
// Calls into functions no group of this round owns (0x446A80, 0x494E70,
// 0x446600) go through raw addresses here and are never bound
// (docs/takeover-queue-round8.md, the rule for calls across groups).
#pragma once

#include <cstdint>

namespace battle_turn_steps {

namespace at {

// The battle's state bytes (PSX 0x801462DC..): the phase's low byte, then the
// sub-states the stubs dispatch on.
constexpr std::uint32_t kPhase = 0x904AA0;        // u8 here (Battle_PhaseDispatch reads the dword's low byte)
constexpr std::uint32_t kState1 = 0x904AA1;       // u8: the phase's step
constexpr std::uint32_t kState2 = 0x904AA2;       // u8: the step's sub-step
constexpr std::uint32_t kState3 = 0x904AA3;       // u8: the result's step
constexpr std::uint32_t kState4 = 0x904AA4;       // u8 (read as a dword and masked): the result's page
constexpr std::uint32_t kCursor = 0x904AA5;       // u8: the member BattleEnd_StartMemberTask is at
constexpr std::uint32_t kRoundFlags = 0x904AA8;   // u16; bits 2, 3 and 4 cleared here, bit 4 tested
constexpr std::uint32_t kEventBattle = 0x904AAA;  // u8 (battle_flow.md: "event battle" is a guess)
constexpr std::uint32_t kMembers = 0x904AB0;      // u8: the party's count
constexpr std::uint32_t kCmdByte = 0x904AB4;      // u8, zeroed with the four below before file 0x131
constexpr std::uint32_t kCmdKind = 0x904ABC;      // u8, set to 8
constexpr std::uint32_t kCmdLong = 0x904ABD;      // u32, zeroed (unaligned)
constexpr std::uint32_t kCmdWord = 0x904AC1;      // u16, zeroed (unaligned)
constexpr std::uint32_t kSideMode = 0x904AE4;     // u8: 2 changes what the faster side gets
constexpr std::uint32_t kMusicFlags = 0x904AE5;   // u8: bit 0x40 keeps the battle's music
constexpr std::uint32_t kBattleEnd = 0x904AE8;    // u8: bit 0 and bit 1 pick the way out (|= 2 when the last enemy falls)
constexpr std::uint32_t kBattleEnd2 = 0x904AE9;   // u8, zeroed on the way out
constexpr std::uint32_t kExtraRound = 0x904B7A;   // u8: 1..3, the faster side's extra round; 0 none
constexpr std::uint32_t kPending = 0x904B82;      // u16: a bit per actor with an effect to show
constexpr std::uint32_t kTimer = 0x904B8E;        // u8 (Battle_ActorSkipped's timer)
constexpr std::uint32_t kRounds = 0x904B90;       // u32: counted up once a round
constexpr std::uint32_t kHookEnd = 0x904B64;      // void (*)(): called as the way out is picked (0x904AE8 bit 1 the win)
constexpr std::uint32_t kHookExit = 0x904B68;     // void (*)(): called on the way out
constexpr std::uint32_t kHookEvent = 0x904B6C;    // unsigned (*)(int): called with 2; al 0xFF holds the round
constexpr std::uint32_t kMusicTrack = 0x904131;   // Music_Track, u8
constexpr std::uint32_t kGameStep = 0x66C7EA;     // Game_Step, u16: counted up on the way out
constexpr std::uint32_t kInput = 0x7E1BEC;        // Input_Pressed, u16

// The party's working records, ObjTrio 0x802D40 stride 0x14C: +0 bit 0
// present, +1 the state byte, +0x89 the character id, +0x90 the status (u16,
// +0x91 its high byte), +0x98 / +0x9A HP and AP, +0x9C, +0xA0 / +0xA2 their
// maxima, +0xAE, +0x130 / +0x134 flags (u32), +0x148 the character record's
// index.
constexpr std::uint32_t kParty = 0x802D40, kPartyStride = 0x14C;
constexpr unsigned kState = 0x01, kCharId = 0x89, kStatus = 0x90, kHp = 0x98, kAp = 0x9A, kByte9C = 0x9C;
constexpr unsigned kMaxHp = 0xA0, kMaxAp = 0xA2, kByteAE = 0xAE, kFlags2 = 0x130, kFlags = 0x134, kRecord = 0x148;
constexpr std::uint32_t kPartyByte = 0x802D20;    // u8, zeroed as the battle's end begins
// The enemies' objects, 0x93B960 stride 0x128; +0x114 flags (u32).
constexpr std::uint32_t kEnemy = 0x93B960, kEnemyStride = 0x128;
constexpr unsigned kEFlags = 0x114;
// A second three-record array of the party's stride (0x939AE0), whose +0x134
// loses bit 0 when a member other than character 4 gets its task.
constexpr std::uint32_t kCopies = 0x939AE0;
// The 48 battle-task slots, 0x84 each; +0x80 the owner.
constexpr std::uint32_t kTasks = 0x93A000, kTaskSize = 0x84;
// Eight dwords 0x803540..0x80355C: while any is not 0 the next round waits.
constexpr std::uint32_t kBusy = 0x803540;
// The twelve message-window records from 0x803160 (0x24 each): byte +0x24 of
// each but the third is zeroed when the battle ends.
constexpr std::uint32_t kWindows = 0x803184, kWindowStride = 0x24;
// CharacterRecords, 0xA4 each: +0xB, +0x10 status (u16), +0x18 HP (u16),
// +0x1C, +0x1E.
constexpr std::uint32_t kRecords = 0x903A70, kRecordStride = 0xA4;

// The dispatch tables (symbols.toml [[data]]).
constexpr std::uint32_t kRoundEndSteps = 0x64AF2C;     // 3, by kState1 - 0x4302B0
constexpr std::uint32_t kRoundEndFaster = 0x64AF38;    // 3, by kState2 - 0x4302C0
constexpr std::uint32_t kEndSteps = 0x64AF44;          // 5, by kState1 - 0x4311E0
constexpr std::uint32_t kEndTaskSteps = 0x64AF58;      // 3, by kState2 - 0x4311F0
constexpr std::uint32_t kEndWinSteps = 0x64AF64;       // 3, by kState2 - 0x4314B0
constexpr std::uint32_t kEndExitSteps = 0x64AF90;      // 4, by kState2 - 0x431760
constexpr std::uint32_t kEndResultSteps = 0x64AFA0;    // 3, by kState3 - 0x431910
constexpr std::uint32_t kEndResultPages = 0x64AFAC;    // 5, by kState4 - 0x431920

}  // namespace at

// Raw addresses of callees no group of this round owns (the rule above).
constexpr std::uint32_t kWriteBackMember = 0x446A80;  // (actor): member's HP / AP / status into its CharacterRecord
                                                      // (PSX Battle_WriteBackMember, called by 0x801D71B0)
constexpr std::uint32_t kClearEnemies = 0x494E70;     // (): bytes +0..+4 of the eight enemy objects zeroed
constexpr std::uint32_t kReloadParty = 0x446600;      // (): each member's +0x80 re-copied from its CharacterRecord

using Step = void (__cdecl*)();

// Every callee that answers in al is typed `unsigned` here and its answer's
// low byte tested by hand: Capcom's leave the upper 24 bits as they fall.
struct Callees {
    unsigned (__cdecl* mark_faster)();                    // Battle_MarkFasterSide (battle_setup.cpp)
    void (__cdecl* tick_counters)();                      // Battle_TickCounters (battle_setup.cpp)
    unsigned (__cdecl* steps[9])();                       // BattleStep_Expire4000 .. _ApUpkeep (battle_setup.cpp)
    void (__cdecl* load_dat)(int);                        // LoadDatFile
    void (__cdecl* clear_commands)();                     // Battle_ClearCommands (battle_damage.cpp)
    unsigned (__cdecl* actor_is_out)(unsigned);           // Battle_ActorIsOut (battle_flow.cpp)
    unsigned (__cdecl* task_create)(unsigned, unsigned);  // BattleTask_Create (battle_flow.cpp)
    unsigned (__cdecl* clear_status)(unsigned, unsigned); // Battle_ClearStatus (battle_setup.cpp)
    void (__cdecl* release_tint)(unsigned char*);         // Sprite_ReleaseTint
    void (__cdecl* return_item)(unsigned);                // Battle_ReturnQueuedItem (battle_setup.cpp)
    void (__cdecl* dropped_call)(unsigned);               // Port_DroppedCall, a bare ret
    void (__cdecl* fade_out)(int);                        // Music_FadeOutStop (sound.cpp)
    void (__cdecl* music_play)(unsigned, int);            // Music_Play (sound.cpp)
    void (__cdecl* open_window)();                        // Battle_OpenMsgWindow (battle_setup.cpp)
    void (__cdecl* opening_message)();                    // Battle_OpeningMessage (battle_setup.cpp)
    int (__cdecl* load_done)();                           // File_LoadDone (mode_flow.cpp), always 1
    void (__cdecl* write_back_member)(unsigned);          // 0x446A80
    void (__cdecl* window_reset)();                       // Window_ResetAll (mode_flow.cpp)
    void (__cdecl* clear_enemies)();                      // 0x494E70
    void (__cdecl* task_clear_all)();                     // BattleTask_ClearAll (battle_flow.cpp)
    void (__cdecl* recalc_stats)(unsigned char*);         // Char_RecalcStats (char_stats.cpp)
    void (__cdecl* reload_party)();                       // 0x446600
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_turn_steps: the start-up fuzz, battle_turn_steps_fuzz.cpp.
// Clones every original before BattleTurnSteps_Inject patches it.
void SelfTest();

}  // namespace battle_turn_steps
