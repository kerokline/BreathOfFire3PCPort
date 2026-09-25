// Internal to battle_win_states.cpp and battle_win_states_fuzz.cpp: every call
// the battle windows' states make, through pointers, so that the start-up fuzz
// can stand recording functions in for them - for the originals' copies and
// for ours alike. docs/battle_win_states.md.
//
// Every callee but two is ours already and is called through its name. The
// two window kinds 6 and 7 of BattleWin_Run's table (0x597C70, 0x597D50) are
// group CM's (docs/takeover-queue-round8.md, "The rule for calls across
// groups") and are called by raw address; so are the bare `ret` 0x437CC0 the
// state tables hold for their idle states.
#pragma once

#include <cstdint>

namespace battle_win_states {

namespace at {

constexpr std::uint32_t kCurrent = 0x905B84;       // the window record Field_RunTaskRecords is running
constexpr std::uint32_t kLabelGate = 0x903A5D;     // u8: the command label is drawn only while 0
constexpr std::uint32_t kChoosing = 0x904AAF;      // u8: non-zero while a target is being chosen
constexpr std::uint32_t kCrossSel = 0x904AB4;      // u8: the command cross's selection
constexpr std::uint32_t kCrossGrow = 0x904ABC;     // u8 x 7: each arm's growth (five arms, two extras)
constexpr std::uint32_t kTarget = 0x939FA0;        // pointer to the target byte: a slot, 0x40 all enemies, 0x80 all members
constexpr std::uint32_t kCrossArms = 0x64E2AC;     // (x, y) bytes per arm, 7 of them
constexpr std::uint32_t kSideDx = 0x64E2BC;        // s8: the target banner's x offset for sides 0 and 3
constexpr std::uint32_t kOtherDx = 0x64E2BE;       // s8: the same for every other side byte
constexpr std::uint32_t kMemberOffsets = 0x64DF70; // BattleWin_MemberTargetOffsets: (dx, dy) s8, 4 per character row
constexpr std::uint32_t kBanners = 0x93B8E0;       // the banner pool: 12 bytes a record, +2 wide, +4 text, +0xA colour
constexpr std::uint32_t kBannerStride = 0xC;
constexpr std::uint32_t kEnemies = 0x93B9E0;       // EnemyWorkingRecords, stride 0x128: slot t is enemy t - 3
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr std::uint32_t kMembers = 0x802D40;       // ObjTrio: the battle's party objects, stride 0x14C
constexpr std::uint32_t kMemberStride = 0x14C;
constexpr std::uint32_t kBareRet = 0x437CC0;       // a bare `ret`: the idle states' handler

// The stack tables, as the originals build them (each `mov [esp + 4 i], imm32`).
constexpr std::uint32_t kKinds[8] = {0x597000, 0x597090, 0x5971B0, 0x597200, 0x597320, 0x5975D0, 0x597C70, 0x597D50};
constexpr std::uint32_t kPartyStates[3] = {0x597030, 0x597070, kBareRet};
constexpr std::uint32_t kCrossStates[3] = {0x5970C0, 0x597160, kBareRet};
constexpr std::uint32_t kLabelStates[3] = {kBareRet, 0x5971E0, kBareRet};
constexpr std::uint32_t kBannerStates[3] = {kBareRet, 0x597230, kBareRet};
constexpr std::uint32_t kEnemyStates[4] = {0x597400, 0x5974C0, 0x597510, kBareRet};
constexpr std::uint32_t kMemberStates[3] = {0x5976D0, 0x597850, 0x5978B0};

}  // namespace at

struct Callees {
    // ours already, by name
    void (__cdecl* party_status)(int, int);                          // BattleWin_DrawPartyStatus 0x442FA0
    void (__cdecl* command_cross)(int, int);                         // BattleWin_DrawCommandCross 0x4432F0
    void (__cdecl* command_label)(unsigned);                         // BattleWin_DrawCommandLabel 0x4439A0
    void (__cdecl* small_box)(int, int);                             // BattleWin_DrawSmallBox 0x443740
    void (__cdecl* medium_box)(int, int);                            // BattleWin_DrawMediumBox 0x443870
    void (__cdecl* target_enemy)(int, int, unsigned);                // BattleWin_DrawTargetEnemy 0x443B10
    void (__cdecl* enemy_status)(int, int, unsigned);                // BattleWin_DrawEnemyStatus 0x443D90
    void (__cdecl* target_member)(int, int, unsigned);               // BattleWin_DrawTargetMember 0x443F60
    void (__cdecl* flag_advance)();                                  // Window_FlagAndAdvance 0x5979E0
    void (__cdecl* restore_back)();                                  // Window_RestoreAndBack 0x597A00
    unsigned (__cdecl* dispatch_kind)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned,
                                      unsigned);                     // Window_DispatchKind 0x597A30
    unsigned char (__cdecl* glyph_count)(const unsigned char*);      // Text_GlyphCount 0x597F40
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt 0x516B30
    void (__cdecl* icon)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);  // Menu_DrawIcon 0x5903F0
    unsigned char (__cdecl* actor_is_out)(unsigned);                 // Battle_ActorIsOut 0x4456C0
    unsigned char (__cdecl* return_true)();                          // Battle_ReturnTrue 0x449E00
    // the stack tables' entries, as addresses
    std::uint32_t kinds[8];
    std::uint32_t party[3];
    std::uint32_t cross[3];
    std::uint32_t label[3];
    std::uint32_t banner[3];
    std::uint32_t enemy[4];
    std::uint32_t member[3];
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_win_states: the start-up fuzz, battle_win_states_fuzz.cpp.
// Clones every original before BattleWinStates_Inject patches it.
void SelfTest();

}  // namespace battle_win_states
