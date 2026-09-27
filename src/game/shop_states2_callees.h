// Internal to shop_states2.cpp and shop_states2_fuzz.cpp: the addresses the
// shop's buy and sell states, the shop's close and task 0's title loop touch,
// and every call they make - through pointers, so that the start-up fuzz can
// stand recording functions in for them, for Capcom's copies and for ours
// alike. docs/shop_states2.md.
//
// Every callee is already a bound name (ours, or Capcom's through its
// symbols.toml signature), so no other group's address is raw here. The
// dispatches are not calls to a named function: the five state stubs jump
// through the tables at 0x664118.. in .data and task 0's loop calls through
// 0x667294; ours read those tables at run time as the originals do, and the
// fuzz swaps their entries for recorders.
#pragma once

#include <cstdint>

namespace shop_states2 {

using U = std::uint32_t;

namespace at {
// The field menus' state block (docs/menu-screens.md): 0x929F00.. bytes.
constexpr U kMenuDone = 0x929F00;     // u8: 1 ends the menu (ShopTrade_Close)
constexpr U kState = 0x929F01;        // u8: the shop's state, 0..4 (ShopTrade_States)
constexpr U kSub = 0x929F02;          // u8: the state's step
constexpr U kCounter = 0x929F04;      // u8: frames before the next step
constexpr U kByte05 = 0x929F05;       // u8, cleared by ShopTrade_OpenWait
constexpr U kMember = 0x929F06;       // s8: the member the equip steps are on
constexpr U kAnswer = 0x929F0B;       // u8: the yes / no cursor, 1 yes
// The shop's own bytes (0x6BC89C..0x6BC8B7).
constexpr U kKind = 0x6BC89C;         // u8: the chosen item's category
constexpr U kUnitPrice = 0x6BC8A0;    // u32: its price, buying (scaled) or selling
constexpr U kSellsEquipment = 0x6BC8A4;   // u8: 1 when the list holds equipment
constexpr U kSlotCursor = 0x6BC8A7;   // u8: 0 / 1, the slot of a two-slot kind (5)
constexpr U kByteA8 = 0x6BC8A8;       // u8, cleared by ShopTrade_OpenWait
constexpr U kRow = 0x6BC8A9;          // s8: the buy list's cursor, then the row sold from
constexpr U kSellFlag = 0x6BC8AA;     // u8: Shop_SellPrice's flag (x50 for 0x2B..0x36)
constexpr U kChoice = 0x6BC8AB;       // u8: buy (0) or sell (1)
constexpr U kCount = 0x6BC8AC;        // u8: how many (read once as a dword)
constexpr U kItem = 0x6BC8AD;         // u8: the chosen item
constexpr U kMaxCount = 0x6BC8B0;     // u32: the most that may be bought or sold
constexpr U kRate = 0x6BC8B4;         // u16: Shop_PriceRate's percentage
// The shop's list: a count byte, then (category, item) byte pairs.
constexpr U kShopList = 0x903844;     // unsigned char *
constexpr U kMoney = 0x904058;        // u32: the party's zenny
constexpr U kMoneyMax = 0x98967F;     // 9,999,999
constexpr U kPartyList = 0x904062;    // u8 per member: the character
constexpr U kRecordOf = 0x66972C;     // u8 per character: its record (symbols.toml's MoveScript_EffectState)
constexpr U kCharRecords = 0x903A70;  // stride 0xA4
constexpr U kCharStride = 0xA4;
// The inventory's lists, by category 0..4: id bytes and count bytes.
constexpr U kIdLists = 0x656B00;      // unsigned char *[5]
constexpr U kCountLists = 0x656B14;   // unsigned char *[5]
// Input.
constexpr U kPressed = 0x7E1BEC;      // Input_Pressed (read as a word, twice as a dword)
constexpr U kConfirm = 0x90358E;      // Field_ConfirmButtons, u16
constexpr U kCancel = 0x903590;       // Field_CancelButtons, u16
// The window records the states fill (Shop_InitWindows sets them up):
// 0x803160 the help line (+0x10 its message word 0x803170), 0x8031CC + 36 n
// the member panels, 0x803238 the slot box, 0x80325C the sell list, 0x803280
// the buy list, 0x8032A4 the buy count, 0x8032C8 the sell count, 0x803430
// the member hand, 0x803454 the cursor hand.
constexpr U kHelp = 0x803170;         // u16: the help line's message
constexpr U kMembers = 0x8031CC;      // 36-byte records, one per party member
constexpr U kMemberStride = 36;
constexpr U kHand = 0x803454;         // u8 on / off, x at 0x803458, y at 0x80345A (u16)
constexpr U kHandX = 0x803458;
constexpr U kHandY = 0x80345A;
// The dispatch tables (symbols.toml [[data]]).
constexpr U kStates = 0x664118;       // ShopTrade_States: 5, by kState
constexpr U kOpenSteps = 0x66412C;    // 2, by kSub
constexpr U kChoiceSteps = 0x664134;  // 1
constexpr U kBuySteps = 0x664138;     // 8
constexpr U kSellSteps = 0x664158;    // 5
constexpr U kTablesEnd = 0x664190;    // Shop_Equip's jump table follows
// Task 0's title loop.
constexpr U kGameMode = 0x66C7E8;     // Game_Mode, u16 (read as a dword, masked)
constexpr U kGameStep = 0x66C7EA;     // Game_Step, u16
constexpr U kModes = 0x667294;        // TitleTask_Modes: 2, by Game_Mode
constexpr U kClutDirty = 0x937F90;    // Gfx_ClutStripDirty, u8
constexpr U kTitleFile = 0x31D;       // the DAT file TitleMode_Load loads
}  // namespace at

struct Callees {
    // The shop
    void (__cdecl* init_windows)();                                     // Shop_InitWindows (ours)
    void (__cdecl* price_rate)(unsigned short*);                        // Shop_PriceRate (ours)
    unsigned (__cdecl* scale_price)(unsigned, unsigned);                // Shop_ScalePrice (ours)
    unsigned (__cdecl* sell_price)(unsigned, unsigned, unsigned);       // Shop_SellPrice (ours)
    void (__cdecl* equip)(unsigned, unsigned, unsigned);                // Shop_Equip (ours)
    // Items and the inventory
    unsigned (__cdecl* icon_kind)(unsigned, unsigned);                  // Item_IconKind (ours)
    unsigned (__cdecl* help_id)(unsigned, unsigned);                    // Item_HelpMessage (ours) - the help line's message
    unsigned (__cdecl* base_price)(unsigned, unsigned);                 // Item_BasePrice (ours)
    unsigned short (__cdecl* inv_count)(unsigned, unsigned, unsigned);  // Inventory_Count (ours)
    unsigned char (__cdecl* inv_add)(unsigned, unsigned, unsigned);     // Inventory_Add (ours)
    unsigned char* (__cdecl* name_ptr)(unsigned, unsigned);             // Item_NamePtr (ours)
    void (__cdecl* text_set)(unsigned, unsigned, const unsigned char*); // TextRecord_Set (ours)
    unsigned (__cdecl* equip_mask)(unsigned, unsigned);                 // Item_EquipMask (ours)
    unsigned char (__cdecl* can_use)(unsigned, unsigned, unsigned, unsigned);   // Item_CanUse (ours)
    void (__cdecl* recalc)(unsigned char*);                             // Char_RecalcStats (ours)
    int (__cdecl* party_count)(unsigned);                               // Party_Count (ours)
    // Input and sound
    unsigned (__cdecl* auto_repeat)(unsigned);                          // Input_AutoRepeat (ours)
    void (__cdecl* sound)(unsigned short);                              // Sound_PlayEffect (ours)
    // Windows, tasks, files
    void (__cdecl* window_reset)();                                     // Window_ResetAll (ours)
    void (__cdecl* task_clear)();                                       // Task_ClearPrivate
    void (__cdecl* task_sleep)(int);                                    // Task_Sleep
    void (__cdecl* run_task_records)();                                 // Field_RunTaskRecords (ours)
    void (__cdecl* load_dat)(int);                                      // LoadDatFile (ours)
    int (__cdecl* load_done)();                                         // File_LoadDone (ours)
    void (__cdecl* clut_restore)();                                     // Gfx_ClutStripRestore (ours)
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=shop_states2: the start-up fuzz (shop_states2_fuzz.cpp).
// Clones every original before ShopStates2_Inject patches it.
void SelfTest();

}  // namespace shop_states2
