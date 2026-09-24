// Internal to battle_setup.cpp and battle_setup_fuzz.cpp: the actor objects'
// fields the battle set-up functions touch, and every call they make - through
// pointers, so that the start-up fuzz can stand recording functions in for
// them, for ours and for the originals' copies alike. docs/battle_setup.md.
//
// Calls into other groups' functions and into unnamed ones go through raw
// addresses here and are never bound (docs/takeover-queue-round7.md, the rule
// for calls across groups): group BB's 0x4456C0, group BE's 0x445980, group
// BF's 0x446FB0 / 0x44A650 / 0x44A6E0 / 0x446BB0, and the unnamed 0x44A910,
// 0x44A960, 0x446CB0, 0x446540, 0x445600 and 0x446D90.
#pragma once

#include <cstdint>

namespace battle_setup {

namespace at {

// The actor objects. Actors 0..2 are the party, ObjTrio 0x802D40 stride 0x14C;
// actors 3..10 the enemies, 0x93B960 stride 0x128 indexed by the actor less 3
// (the enemy's name record, EnemyWorkingRecords 0x93B9E0, is its +0x80). The
// PSX's are 0x80145E8C stride 0x140 and 0x801EB540 stride 0x118.
constexpr std::uint32_t kParty = 0x802D40, kPartyStride = 0x14C;
constexpr std::uint32_t kEnemy = 0x93B960, kEnemyStride = 0x128;
// Both kinds: +0 bit 0 present; +1..+4 the actor's state bytes (6, 5, 0, 0 at
// an effect, 6, 4, 4 when the AP ran out).
constexpr unsigned kPresent = 0x00, kState1 = 0x01, kState2 = 0x02, kState3 = 0x03, kState4 = 0x04;
constexpr unsigned kPos = 0x34;          // 12 bytes restored by Battle_ClearStatus's bit 0x20
// The party's: the persistent record's copy is at +0x80 (its offsets + 0x80).
constexpr unsigned kPCharId = 0x89;      // u8, record +0x09
constexpr unsigned kPStatus = 0x90;      // u16, record +0x10; +0x91 its high byte
constexpr unsigned kPWeapon = 0x92;      // u8, record +0x12
constexpr unsigned kPArmour3 = 0x95;     // u8, record +0x15, the third armour byte
constexpr unsigned kPAccessory = 0x96;   // u8 x2, record +0x16 / +0x17
constexpr unsigned kPHp = 0x98, kPAp = 0x9A;          // u16, record +0x18 / +0x1A
constexpr unsigned kPByte9C = 0x9C;      // u8, record +0x1C, set from +0xAE
constexpr unsigned kPMaxHp = 0xA0, kPMaxAp = 0xA2;    // u16, record +0x20 / +0x22
constexpr unsigned kPAgi = 0xA8;         // u16, record +0x28
constexpr unsigned kPByteAE = 0xAE;      // u8, record +0x2E
constexpr unsigned kPQueuedKind = 0x125; // u8: 5 when the queued action is an item
constexpr unsigned kPQueuedItem = 0x126; // u16: category << 8 | id - Battle_ReturnQueuedItem
constexpr unsigned kPHpChange = 0x128, kPApChange = 0x12A;   // u16 each
constexpr unsigned kPEffect = 0x12C;     // u8: 0x11 or 2 with an effect
constexpr unsigned kPCounter = 0x12D;    // u8: the bit-0x40 / 0x20 statuses' counter (0x446CB0 reads it)
constexpr unsigned kPItemSlot = 0x12E;   // u8
constexpr unsigned kPFlags = 0x134;      // u32
constexpr unsigned kPCount4000 = 0x142, kPCount800 = 0x143;  // u8 counters
constexpr unsigned kPCount40 = 0x144, kPCount80 = 0x145;     // u8, zeroed with flags 0x40 / 0x80
// The enemies': the record at +0x80 is the PSX's + 0x10.
constexpr unsigned kEStatus = 0x92;      // u16
constexpr unsigned kEHp = 0xA4, kEMaxHp = 0xB0, kEAgi = 0xB8;   // u16
constexpr unsigned kEHpChange = 0x108, kEApChange = 0x10A;      // u16
constexpr unsigned kEEffect = 0x10C, kECounter = 0x10D;         // u8
constexpr unsigned kEFlags = 0x114;      // u32
constexpr unsigned kECount4000 = 0x122, kECount40 = 0x124, kECount80 = 0x125;   // u8

constexpr std::uint32_t kHpDriftMode = 0x904060;  // u8: at 5 every member's HP change starts at -1 (BattleStep_HpDrift)
constexpr std::uint32_t kItemQueue = 0x904ACC;    // u8 per slot: the acting actors, 0x904AE2 - 1 .. 0x904AE3
constexpr std::uint32_t kQueueFrom = 0x904AE2, kQueueTo = 0x904AE3;
constexpr std::uint32_t kActing = 0x904B34;       // u32: the acting actor in its low byte, the command kind above
constexpr std::uint32_t kApCost = 0x904B78;       // u32, its low byte the AP cost doubled
constexpr std::uint32_t kAbility = 0x904B80;      // u16
constexpr std::uint32_t kPending = 0x904B82;      // u16: a bit per actor with an effect to show (0x446FB0 sets it)
constexpr std::uint32_t kField12 = 0x904B89;      // u8, 0x12 with character id 0 / 7
constexpr std::uint32_t kTimer = 0x904B8E;        // u8: counts down a turn at a time; flag 0x10 matters while it runs
constexpr std::uint32_t kOpening = 0x904B90;      // u32: picks Battle_OpeningMessage's lines
constexpr std::uint32_t kSavedPos = 0x93A034, kSavedPosStride = 0x84;   // 12 bytes per actor
constexpr std::uint32_t kMsgWindow = 0x803162;    // the message window's four fields Battle_OpenMsgWindow sets

inline std::uint32_t Party(unsigned actor) { return kParty + (actor & 0xFF) * kPartyStride; }
inline std::uint32_t Enemy(unsigned actor) { return kEnemy + (((actor & 0xFF) - 3u) & 0xFFFFFFFFu) * kEnemyStride; }

}  // namespace at

// Raw addresses of the callees this group does not own (the rule above).
constexpr std::uint32_t kActorAbsent = 0x4456C0;     // BB: (actor) -> al, 1 when not present (+0 bit 0) or with status 0x4000
constexpr std::uint32_t kActorCanAct = 0x445980;     // BE: (actor) -> al
constexpr std::uint32_t kSetPending = 0x446FB0;      // BF: (actor): kPending |= 1 << actor
constexpr std::uint32_t kMessage = 0x44A650;         // BF: (a, b, c, d, text) a battle message line, bytes + a pointer
constexpr std::uint32_t kMessageAt = 0x44A6E0;       // BF: (slot, a, b, c, d, text) the same at a given slot
constexpr std::uint32_t kStatusTint = 0x446BB0;      // BF: (status): a tint on Sprite_Current when bit 0x80 is set
constexpr std::uint32_t kPartyName = 0x44A910;       // (actor): the member's name into Text_Records[0]
constexpr std::uint32_t kEnemyName = 0x44A960;       // (actor): the enemy's 12-byte name into Text_Records[0]
constexpr std::uint32_t kWakeRoll = 0x446CB0;        // (actor) -> al: the status counter's roll (Rand)
constexpr std::uint32_t kSetHpChange = 0x446540;     // (actor): the HP change word from HP when status 0x80
constexpr std::uint32_t kEnemyOutpaces = 0x445600;   // (actor, average, highest) -> al, 0x4455C0's enemy twin
constexpr std::uint32_t kReturnItem = 0x446D90;      // (slot, item) -> al: one back into the inventory

// Every callee that answers in al is typed `unsigned` here and its answer's
// low byte tested by hand: Capcom's leave the upper 24 bits as they fall, so
// nothing of ours may assume they are extended.
struct Callees {
    unsigned (__cdecl* actor_absent)(unsigned);                                  // 0x4456C0, BB
    unsigned (__cdecl* can_act)(unsigned);                                       // 0x445980, BE
    void (__cdecl* set_pending)(unsigned);                                            // 0x446FB0, BF
    void (__cdecl* message)(unsigned, unsigned, unsigned, unsigned, const unsigned char*);            // 0x44A650, BF
    void (__cdecl* message_at)(unsigned, unsigned, unsigned, unsigned, unsigned, const unsigned char*); // 0x44A6E0, BF
    void (__cdecl* status_tint)(unsigned);                                            // 0x446BB0, BF
    void (__cdecl* party_name)(unsigned);                                             // 0x44A910
    void (__cdecl* enemy_name)(unsigned);                                             // 0x44A960
    unsigned (__cdecl* wake_roll)(unsigned);                                     // 0x446CB0
    void (__cdecl* set_hp_change)(unsigned);                                          // 0x446540
    unsigned (__cdecl* enemy_outpaces)(unsigned, unsigned, unsigned);            // 0x445600
    unsigned (__cdecl* return_item)(unsigned, unsigned);                         // 0x446D90
    void (__cdecl* release_tint)(unsigned char*);                                     // Sprite_ReleaseTint 0x454DC0
    const unsigned char* (__cdecl* msg)(unsigned);                                    // Msg_SystemPtr (msg_pool.cpp)
    unsigned (__cdecl* window_alloc)(unsigned, unsigned);                             // Window_Alloc (msgbox.cpp)
    int (__cdecl* rand)();                                                            // Rand, the CRT's
    // ours, in this module
    unsigned (__cdecl* skipped)(unsigned);                                       // Battle_ActorSkipped
    void (__cdecl* open_window)();                                                    // Battle_OpenMsgWindow
    unsigned (__cdecl* clear_status)(unsigned, unsigned);                             // Battle_ClearStatus
    unsigned (__cdecl* standing)(unsigned);                                           // Battle_ActorStanding
    unsigned (__cdecl* outpaces)(unsigned, unsigned, unsigned);                       // Battle_PartyOutpaces
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_setup: the start-up fuzz, battle_setup_fuzz.cpp. Clones
// every original before BattleSetup_Inject patches it.
void SelfTest();

}  // namespace battle_setup
