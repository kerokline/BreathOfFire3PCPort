// BOF3X_SHADOW=magic_s03: group S03's three overlays (MAGIC006, 009, 012)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s03.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC006 / 009 / 012 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// libgpu entry points, Gte_RotTransPers3 / Gte_PrimDepths3_10B, Math_Sin /
// Math_Cos / Math_Ratan2, Gfx_CommitPrim, MapView_LinkPrimAt),
// Battle_ActorIsOut, the engine's 0x446770, and the functions of its own that
// its functions call directly. Everything the harness lacks is built here, not
// in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - Gte_RotTransPers3 logs its three SVECTORs (`deref`);
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - the three callees answering a flag the callers re-read state after
//     (Battle_ActorIsOut, MagicFx_NearSprite, Sprite_ScriptTickOnce) note a
//     marker and stir again: a kFlag recorder's own disturbance is empty
//     exactly when it answers 0 (both come from one hash), and the note grows
//     the log, so the second disturbance comes from a new hash - the kFlag
//     blind spot of group E, worked round here;
//   - MagicFx_NearSprite answers kBool: both callers test the whole of eax;
//   - Battle_ActorIsOut (for BlitzBolt_Seek) and Math_Cos (for
//     MindSwordSpark_Start) also move what their caller reads back after
//     them: the bolt's record's point, the angle word 0x903858;
//   - Math_Ratan2, while MindSwordBlade_Fly is fuzzed, answers half the time
//     one step either side of 0x600 / 0xA00 from the heading it replaces;
//   - the group's settle keeps the bolt's record index +4 inside its side's
//     records, and ChlorineCloud_Start's offset index +0xB inside its table,
//     while those functions are fuzzed (both are read again after a call; past
//     them the original reads outside the image or its table and ours aborts).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s03.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s03 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC006 / 009 / 012 --clones, 2026-09-26, names
// given.
// 0x49C3D0: 0x26 bytes  MindSword_Task
constexpr mh::Imm kImms49C3D0[] = {{0xF, 0x49C400}, {0x17, 0x4F7350}};
// 0x49C400: 0xB7 bytes  MindSword_Start
constexpr mh::CallSite kCalls49C400[] = {{0x6C, 0x435180}};
// 0x49C4C0: 0x12 bytes; +0xB note: jmp through .data 0x65a5e0 (a data_tables entry)  MindSwordChild_Task
// 0x49C4E0: 0x77 bytes  MindSwordBlade_Run
constexpr mh::CallSite kCalls49C4E0[] = {{0x53, 0x4FBD10}, {0x65, 0x49CF10}, {0x6E, 0x49C910}};
constexpr mh::Imm kImms49C4E0[] = {{0xF, 0x49C560}, {0x17, 0x49C680}, {0x22, 0x49C6C0}, {0x2A, 0x49C6F0}, {0x32, 0x49C830}, {0x3A, 0x4B1740}};
// 0x49C560: 0x11E bytes  MindSwordBlade_Appear
constexpr mh::CallSite kCalls49C560[] = {{0x53, 0x4FBD10}, {0x89, 0x5A7A70}, {0xD7, 0x5A7A70}, {0x101, 0x587900}};
// 0x49C680: 0x3C bytes  MindSwordBlade_Spin
// 0x49C6C0: 0x2F bytes  MindSwordBlade_Wait
constexpr mh::CallSite kCalls49C6C0[] = {{0x1E, 0x587900}};
// 0x49C6F0: 0x134 bytes  MindSwordBlade_Fly
constexpr mh::CallSite kCalls49C6F0[] = {{0x44, 0x4FBA90}, {0x83, 0x5A7A70}, {0x97, 0x4FBC30}, {0xB7, 0x452F70}, {0x11D, 0x452F70}};
// 0x49C830: 0xDD bytes  MindSwordBlade_Burst
constexpr mh::CallSite kCalls49C830[] = {{0x19, 0x435180}, {0x51, 0x5B93D2}, {0x78, 0x435180}, {0xB0, 0x587900}, {0xD6, 0x4351F0}};
// 0x49C910: 0x600 bytes  MindSwordBlade_DrawLead
constexpr mh::CallSite kCalls49C910[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x55, 0x5A7610}, {0x5D, 0x5A7780}, {0x96, 0x5A7A00}, {0xC4, 0x5A7A50}, {0x108, 0x5A7A00}, {0x136, 0x5A7A50}, {0x17B, 0x5A7A00}, {0x1A9, 0x5A7A50}, {0x221, 0x461E50}, {0x244, 0x5A77C0}, {0x24D, 0x461E50}, {0x259, 0x5A7610}, {0x261, 0x5A7780}, {0x28B, 0x5A7A00}, {0x2B9, 0x5A7A50}, {0x307, 0x5A7A00}, {0x335, 0x5A7A50}, {0x3A5, 0x5A7A00}, {0x3D3, 0x5A7A50}, {0x42C, 0x461E50}, {0x438, 0x5A7610}, {0x440, 0x5A7780}, {0x46A, 0x5A7A00}, {0x498, 0x5A7A50}, {0x4E1, 0x5A7A00}, {0x50F, 0x5A7A50}, {0x572, 0x5A7A00}, {0x5A0, 0x5A7A50}, {0x5F3, 0x461E50}};
// 0x49CF10: 0x3EC bytes  MindSwordBlade_Draw
constexpr mh::CallSite kCalls49CF10[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x4A, 0x5A7610}, {0x52, 0x5A7780}, {0x7C, 0x5A7A00}, {0xAA, 0x5A7A50}, {0xF5, 0x5A7A00}, {0x123, 0x5A7A50}, {0x193, 0x5A7A00}, {0x1C1, 0x5A7A50}, {0x218, 0x461E50}, {0x224, 0x5A7610}, {0x22C, 0x5A7780}, {0x256, 0x5A7A00}, {0x284, 0x5A7A50}, {0x2CF, 0x5A7A00}, {0x2FD, 0x5A7A50}, {0x360, 0x5A7A00}, {0x38E, 0x5A7A50}, {0x3E0, 0x461E50}};
// 0x49D300: 0x49 bytes  MindSwordSpark_Run
constexpr mh::CallSite kCalls49D300[] = {{0x3B, 0x4FBD10}, {0x40, 0x49D430}};
constexpr mh::Imm kImms49D300[] = {{0xF, 0x49D350}, {0x17, 0x49D400}, {0x22, 0x4A5180}};
// 0x49D350: 0xA2 bytes  MindSwordSpark_Start
constexpr mh::CallSite kCalls49D350[] = {{0x36, 0x5A7A50}, {0x5A, 0x5A7A00}};
// 0x49D400: 0x2E bytes  MindSwordSpark_Grow
// 0x49D430: 0x181 bytes  MindSwordSpark_Draw
constexpr mh::CallSite kCalls49D430[] = {{0x48, 0x5A77C0}, {0x5E, 0x572FA0}, {0x6A, 0x5A7610}, {0x71, 0x5A7780}, {0x175, 0x572FA0}};
// 0x49D5C0: 0x56 bytes  MindSwordFlash_Run
constexpr mh::CallSite kCalls49D5C0[] = {{0x43, 0x4B7D40}, {0x48, 0x49D680}, {0x4D, 0x5A7BC0}};
constexpr mh::Imm kImms49D5C0[] = {{0xF, 0x49D620}, {0x17, 0x49D660}, {0x22, 0x4C32D0}, {0x2A, 0x4B1740}};
// 0x49D620: 0x3C bytes  MindSwordFlash_Start
// 0x49D660: 0x1D bytes  MindSwordFlash_Grow
// 0x49D680: 0x1B2 bytes  MindSwordFlash_Draw
constexpr mh::CallSite kCalls49D680[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x5D, 0x5A7A00}, {0x76, 0x5A7A50}, {0xAD, 0x5A75F0}, {0xB4, 0x5A7780}, {0xE2, 0x5A7A00}, {0xFB, 0x5A7A50}, {0x138, 0x5A84A0}, {0x13E, 0x5A9310}, {0x173, 0x461E50}, {0x199, 0x5A77C0}, {0x1A2, 0x461E50}};
// 0x49D840: 0x3E bytes  Chlorine_Task
constexpr mh::Imm kImms49D840[] = {{0xF, 0x49D880}, {0x17, 0x49D9B0}, {0x22, 0x4DA3B0}, {0x2A, 0x49DA50}, {0x32, 0x49DA60}};
// 0x49D880: 0x129 bytes  Chlorine_Start
constexpr mh::CallSite kCalls49D880[] = {{0x53, 0x4FB830}, {0x5C, 0x435180}};
// 0x49D9B0: 0x96 bytes  Chlorine_Release
constexpr mh::CallSite kCalls49D9B0[] = {{0x18, 0x435180}, {0x67, 0x4FB830}, {0x79, 0x587900}};
// 0x49DA50: 0x10 bytes  Chlorine_WaitChildren
// 0x49DA60: 0x1C bytes  Chlorine_End
constexpr mh::CallSite kCalls49DA60[] = {{0xF, 0x4530D0}, {0x17, 0x4351F0}};
// 0x49DA80: 0x12 bytes; +0xB note: jmp through .data 0x65a5f4 (a data_tables entry)  ChlorineChild_Task
// 0x49DAA0: 0x29 bytes; +0xB note: call through .data 0x65a5fc (a data_tables entry)  ChlorineCloud_Run
constexpr mh::CallSite kCalls49DAA0[] = {{0x23, 0x49DBC0}};
// 0x49DAD0: 0xBF bytes  ChlorineCloud_Start
constexpr mh::CallSite kCalls49DAD0[] = {{0x61, 0x446770}, {0xA6, 0x4FBD10}};
// 0x49DB90: 0x2B bytes  ChlorineCloud_Grow
// 0x49DBC0: 0x233 bytes  ChlorineCloud_Draw
constexpr mh::CallSite kCalls49DBC0[] = {{0x10, 0x5A77C0}, {0x26, 0x572FA0}, {0x44, 0x5A75D0}, {0x4C, 0x5A7780}, {0x56, 0x5A7A50}, {0x81, 0x5A7A00}, {0xAC, 0x5A7A50}, {0xD7, 0x5A7A00}, {0x105, 0x5A7A50}, {0x130, 0x5A7A00}, {0x15B, 0x5A7A50}, {0x186, 0x5A7A00}, {0x1BA, 0x5A79A0}, {0x1CA, 0x5A79E0}, {0x228, 0x572FA0}};
// 0x49DE00: 0x46 bytes  ChlorineCopy_Task
constexpr mh::CallSite kCalls49DE00[] = {{0x3D, 0x588F20}};
constexpr mh::Imm kImms49DE00[] = {{0xF, 0x49DE50}, {0x17, 0x49DE70}, {0x22, 0x49DEC0}, {0x2A, 0x4AEE90}};
// 0x49DE50: 0x17 bytes  ChlorineCopy_Size
constexpr mh::CallSite kCalls49DE50[] = {{0x0, 0x4FC260}};
// 0x49DE70: 0x46 bytes  ChlorineCopy_Play
constexpr mh::CallSite kCalls49DE70[] = {{0x22, 0x4FC030}, {0x2A, 0x589410}};
// 0x49DEC0: 0x2F bytes  ChlorineCopy_Wait
// 0x49E000: 0x26 bytes  Blitz_Task
constexpr mh::Imm kImms49E000[] = {{0xF, 0x49E030}, {0x17, 0x49E1C0}};
// 0x49E030: 0x18D bytes  Blitz_Start
constexpr mh::CallSite kCalls49E030[] = {{0x73, 0x4456C0}, {0x83, 0x435180}, {0xE5, 0x4456C0}, {0xF5, 0x435180}};
// 0x49E1C0: 0x7F bytes  Blitz_End
constexpr mh::CallSite kCalls49E1C0[] = {{0x79, 0x4351F0}};
// 0x49E240: 0x12 bytes; +0xB note: jmp through .data 0x65a62c (a data_tables entry)  BlitzBolt_Task
// 0x49E260: 0x12 bytes; +0xB note: jmp through .data 0x65a630 (a data_tables entry)  BlitzBolt_Run
// 0x49E280: 0xBC bytes  BlitzBolt_Start
constexpr mh::CallSite kCalls49E280[] = {{0x49, 0x446770}, {0x90, 0x5B93D2}};
// 0x49E340: 0x1AA bytes  BlitzBolt_Seek
constexpr mh::CallSite kCalls49E340[] = {{0x7E, 0x4456C0}, {0xE7, 0x4FBA90}, {0xF2, 0x4FBC30}, {0x113, 0x4530D0}, {0x11D, 0x587900}, {0x157, 0x5A7A70}, {0x189, 0x4FBD10}, {0x19A, 0x49E720}};
// 0x49E4F0: 0x9F bytes  BlitzBolt_Bounce
constexpr mh::CallSite kCalls49E4F0[] = {{0x1A, 0x5A7A00}, {0x37, 0x5A7A50}, {0x85, 0x4FBD10}, {0x96, 0x49E720}};
// 0x49E590: 0x10D bytes  BlitzBolt_Next
constexpr mh::CallSite kCalls49E590[] = {{0x9, 0x4456C0}, {0x98, 0x446770}, {0xE1, 0x5B93D2}};
// 0x49E6A0: 0x7B bytes  BlitzBolt_Drift
constexpr mh::CallSite kCalls49E6A0[] = {{0x39, 0x4FBA90}, {0x5D, 0x4FBD10}, {0x6E, 0x49E720}};
// 0x49E720: 0x2C3 bytes  BlitzBolt_Draw
constexpr mh::CallSite kCalls49E720[] = {{0x24, 0x5A77C0}, {0x3A, 0x572FA0}, {0x64, 0x5A75D0}, {0x6B, 0x5A7780}, {0x87, 0x5A7A50}, {0xB3, 0x5A7A00}, {0xF0, 0x5A7A50}, {0x11C, 0x5A7A00}, {0x15C, 0x5A7A50}, {0x188, 0x5A7A00}, {0x1C5, 0x5A7A50}, {0x1F1, 0x5A7A00}, {0x224, 0x5A79A0}, {0x234, 0x5A79E0}, {0x2B8, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S03_CLONE(name, base, size, calls, n_calls, imms, n_imms) \
    {#name, base, size, calls, n_calls, imms, n_imms, nullptr, 0, reinterpret_cast<const void*>(&::name)}
const mh::Clone kClones[] = {
    S03_CLONE(MindSword_Task, 0x49C3D0, 0x26, nullptr, 0, kImms49C3D0, MH_N(kImms49C3D0)),
    S03_CLONE(MindSword_Start, 0x49C400, 0xB7, kCalls49C400, MH_N(kCalls49C400), nullptr, 0),
    S03_CLONE(MindSwordChild_Task, 0x49C4C0, 0x12, nullptr, 0, nullptr, 0),
    S03_CLONE(MindSwordBlade_Run, 0x49C4E0, 0x77, kCalls49C4E0, MH_N(kCalls49C4E0), kImms49C4E0, MH_N(kImms49C4E0)),
    S03_CLONE(MindSwordBlade_Appear, 0x49C560, 0x11E, kCalls49C560, MH_N(kCalls49C560), nullptr, 0),
    S03_CLONE(MindSwordBlade_Spin, 0x49C680, 0x3C, nullptr, 0, nullptr, 0),
    S03_CLONE(MindSwordBlade_Wait, 0x49C6C0, 0x2F, kCalls49C6C0, MH_N(kCalls49C6C0), nullptr, 0),
    S03_CLONE(MindSwordBlade_Fly, 0x49C6F0, 0x134, kCalls49C6F0, MH_N(kCalls49C6F0), nullptr, 0),
    S03_CLONE(MindSwordBlade_Burst, 0x49C830, 0xDD, kCalls49C830, MH_N(kCalls49C830), nullptr, 0),
    S03_CLONE(MindSwordBlade_DrawLead, 0x49C910, 0x600, kCalls49C910, MH_N(kCalls49C910), nullptr, 0),
    S03_CLONE(MindSwordBlade_Draw, 0x49CF10, 0x3EC, kCalls49CF10, MH_N(kCalls49CF10), nullptr, 0),
    S03_CLONE(MindSwordSpark_Run, 0x49D300, 0x49, kCalls49D300, MH_N(kCalls49D300), kImms49D300, MH_N(kImms49D300)),
    S03_CLONE(MindSwordSpark_Start, 0x49D350, 0xA2, kCalls49D350, MH_N(kCalls49D350), nullptr, 0),
    S03_CLONE(MindSwordSpark_Grow, 0x49D400, 0x2E, nullptr, 0, nullptr, 0),
    S03_CLONE(MindSwordSpark_Draw, 0x49D430, 0x181, kCalls49D430, MH_N(kCalls49D430), nullptr, 0),
    S03_CLONE(MindSwordFlash_Run, 0x49D5C0, 0x56, kCalls49D5C0, MH_N(kCalls49D5C0), kImms49D5C0, MH_N(kImms49D5C0)),
    S03_CLONE(MindSwordFlash_Start, 0x49D620, 0x3C, nullptr, 0, nullptr, 0),
    S03_CLONE(MindSwordFlash_Grow, 0x49D660, 0x1D, nullptr, 0, nullptr, 0),
    S03_CLONE(MindSwordFlash_Draw, 0x49D680, 0x1B2, kCalls49D680, MH_N(kCalls49D680), nullptr, 0),
    S03_CLONE(Chlorine_Task, 0x49D840, 0x3E, nullptr, 0, kImms49D840, MH_N(kImms49D840)),
    S03_CLONE(Chlorine_Start, 0x49D880, 0x129, kCalls49D880, MH_N(kCalls49D880), nullptr, 0),
    S03_CLONE(Chlorine_Release, 0x49D9B0, 0x96, kCalls49D9B0, MH_N(kCalls49D9B0), nullptr, 0),
    S03_CLONE(Chlorine_WaitChildren, 0x49DA50, 0x10, nullptr, 0, nullptr, 0),
    S03_CLONE(Chlorine_End, 0x49DA60, 0x1C, kCalls49DA60, MH_N(kCalls49DA60), nullptr, 0),
    S03_CLONE(ChlorineChild_Task, 0x49DA80, 0x12, nullptr, 0, nullptr, 0),
    S03_CLONE(ChlorineCloud_Run, 0x49DAA0, 0x29, kCalls49DAA0, MH_N(kCalls49DAA0), nullptr, 0),
    S03_CLONE(ChlorineCloud_Start, 0x49DAD0, 0xBF, kCalls49DAD0, MH_N(kCalls49DAD0), nullptr, 0),
    S03_CLONE(ChlorineCloud_Grow, 0x49DB90, 0x2B, nullptr, 0, nullptr, 0),
    S03_CLONE(ChlorineCloud_Draw, 0x49DBC0, 0x233, kCalls49DBC0, MH_N(kCalls49DBC0), nullptr, 0),
    S03_CLONE(ChlorineCopy_Task, 0x49DE00, 0x46, kCalls49DE00, MH_N(kCalls49DE00), kImms49DE00, MH_N(kImms49DE00)),
    S03_CLONE(ChlorineCopy_Size, 0x49DE50, 0x17, kCalls49DE50, MH_N(kCalls49DE50), nullptr, 0),
    S03_CLONE(ChlorineCopy_Play, 0x49DE70, 0x46, kCalls49DE70, MH_N(kCalls49DE70), nullptr, 0),
    S03_CLONE(ChlorineCopy_Wait, 0x49DEC0, 0x2F, nullptr, 0, nullptr, 0),
    S03_CLONE(Blitz_Task, 0x49E000, 0x26, nullptr, 0, kImms49E000, MH_N(kImms49E000)),
    S03_CLONE(Blitz_Start, 0x49E030, 0x18D, kCalls49E030, MH_N(kCalls49E030), nullptr, 0),
    S03_CLONE(Blitz_End, 0x49E1C0, 0x7F, kCalls49E1C0, MH_N(kCalls49E1C0), nullptr, 0),
    S03_CLONE(BlitzBolt_Task, 0x49E240, 0x12, nullptr, 0, nullptr, 0),
    S03_CLONE(BlitzBolt_Run, 0x49E260, 0x12, nullptr, 0, nullptr, 0),
    S03_CLONE(BlitzBolt_Start, 0x49E280, 0xBC, kCalls49E280, MH_N(kCalls49E280), nullptr, 0),
    S03_CLONE(BlitzBolt_Seek, 0x49E340, 0x1AA, kCalls49E340, MH_N(kCalls49E340), nullptr, 0),
    S03_CLONE(BlitzBolt_Bounce, 0x49E4F0, 0x9F, kCalls49E4F0, MH_N(kCalls49E4F0), nullptr, 0),
    S03_CLONE(BlitzBolt_Next, 0x49E590, 0x10D, kCalls49E590, MH_N(kCalls49E590), nullptr, 0),
    S03_CLONE(BlitzBolt_Drift, 0x49E6A0, 0x7B, kCalls49E6A0, MH_N(kCalls49E6A0), nullptr, 0),
    S03_CLONE(BlitzBolt_Draw, 0x49E720, 0x2C3, kCalls49E720, MH_N(kCalls49E720), nullptr, 0),
};
#undef S03_CLONE
#undef MH_N

enum : unsigned {
    kMindSword_Task, kMindSword_Start, kMindSwordChild_Task, kMindSwordBlade_Run, kMindSwordBlade_Appear,
    kMindSwordBlade_Spin, kMindSwordBlade_Wait, kMindSwordBlade_Fly, kMindSwordBlade_Burst, kMindSwordBlade_DrawLead,
    kMindSwordBlade_Draw, kMindSwordSpark_Run, kMindSwordSpark_Start, kMindSwordSpark_Grow, kMindSwordSpark_Draw,
    kMindSwordFlash_Run, kMindSwordFlash_Start, kMindSwordFlash_Grow, kMindSwordFlash_Draw,
    kChlorine_Task, kChlorine_Start, kChlorine_Release, kChlorine_WaitChildren, kChlorine_End, kChlorineChild_Task,
    kChlorineCloud_Run, kChlorineCloud_Start, kChlorineCloud_Grow, kChlorineCloud_Draw, kChlorineCopy_Task,
    kChlorineCopy_Size, kChlorineCopy_Play, kChlorineCopy_Wait,
    kBlitz_Task, kBlitz_Start, kBlitz_End, kBlitzBolt_Task, kBlitzBolt_Run, kBlitzBolt_Start, kBlitzBolt_Seek,
    kBlitzBolt_Bounce, kBlitzBolt_Next, kBlitzBolt_Drift, kBlitzBolt_Draw, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850;

// Set by the seed for the one function each matters to.
unsigned g_k = kCount;                // the function being fuzzed
bool Fuzzing(unsigned k) { return g_k == k; }

unsigned char* Sc() { return Sprite_Current; }
unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }

// The group's settle: the two indexes its functions read again after a call
// kept inside what they index, for the functions that read them.
void Settle() {
    if (Fuzzing(kBlitzBolt_Seek) || Fuzzing(kBlitzBolt_Next))
        Sc()[4] = Byte(Sc()[4] % (mh::Mem(mh::at::kTarget)[0] & 0x40 ? 8u : 5u));
    if (Fuzzing(kChlorineCloud_Start)) Sc()[0xB] = Byte(Sc()[0xB] % 3);
}

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer (a draw writes up to 0x48 past it).
void Advance(std::uint32_t size) {
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[3]);
    return answer;
}
// 0x446770 turns the task's +0xC / +0x10 by its +8: the inputs logged, a new
// pair written where the real one writes.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(task[8], static_cast<std::uint32_t>(Long(task + 0xC)), static_cast<std::uint32_t>(Long(task + 0x10)));
    mh::FillBytes(task + 0xC, 8);
    return answer;
}
// The flag callees: a marker in the log, then the harness's disturbance from
// the new hash (see the header comment).
std::uint32_t StirAgain(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(0x503);
    mh::Stir();
    return answer;
}
// Battle_ActorIsOut while BlitzBolt_Seek is fuzzed: after the stir, half the
// time the bolt's record's point (+0x34..+0x3F) moved, so the out branch's
// reads of it after the call are compared (the harness writes a record only
// when it is the target enemy's).
std::uint32_t OutEffect(const std::uint32_t* a, std::uint32_t answer) {
    answer = StirAgain(a, answer);
    if (Fuzzing(kBlitzBolt_Seek) && (mh::Noise() & 1)) {
        const unsigned i = Sc()[4];
        unsigned char* const rec = mh::Mem(mh::at::kTarget)[0] & 0x40 ? mh::Mem(mh::at::kEnemies + (i % 8) * mh::at::kEnemyStride)
                                                                       : mh::PartyOf(Byte(i % 5));
        mh::FillBytes(rec + 0x34, 12);
    }
    return answer;
}
// Math_Cos while MindSwordSpark_Start is fuzzed: half the time the angle word
// 0x903858 moved, so the sine's read of it back is compared.
std::uint32_t CosEffect(const std::uint32_t*, std::uint32_t answer) {
    if (Fuzzing(kMindSwordSpark_Start) && (mh::Noise() & 1)) mh::FillBytes(mh::Mem(kScratch + 8), 2);
    return answer;
}
// Math_Ratan2 while MindSwordBlade_Fly is fuzzed: half the time the heading
// kept in +0x10 turned by one step either side of 0x600 / 0xA00.
std::uint32_t HeadingEffect(const std::uint32_t*, std::uint32_t answer) {
    if (!Fuzzing(kMindSwordBlade_Fly)) return answer;
    const std::uint32_t h = mh::Noise();
    if (h & 1) return answer;
    static const std::uint32_t kTurns[] = {0x5FF, 0x600, 0x601, 0x9FF, 0xA00, 0xA01};
    const std::uint32_t turn = kTurns[(h >> 1) % 6];
    const std::uint32_t kept = static_cast<std::uint32_t>(Long(Sc() + 0x10));
    return (answer & 0xFFFFF000u) | ((h & 0x100 ? kept - turn : kept + turn) & 0xFFF);
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S03_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S03_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    // MagicFx_NearSprite answers an int its two callers here test whole: kBool
    {S03_OURS(MagicFx_NearSprite), 2, {kAll, kAll}, mh::Answer::kBool, 0, 0, {}, &StirAgain},
    {S03_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &StirAgain},
    {S03_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &OutEffect},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map: all ours)
    {S03_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S03_OURS(Math_Cos), 1, {kAll}, kG, 0, 0, {}, &CosEffect},
    {S03_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0, {}, &HeadingEffect},
    {S03_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S03_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S03_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S03_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S03_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S03_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S03_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S03_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S03_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    // Gte_RotTransPers3: the three SVECTORs by their six bytes, the outputs by
    // address, the depth and flag (the caller's stack) not at all
    {S03_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S03_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S03_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // this group's own, called directly
    {S03_RAW(0x49C910), 0, {}, kG, 0, 0},
    {S03_RAW(0x49CF10), 0, {}, kG, 0, 0},
    {S03_RAW(0x49D430), 0, {}, kG, 0, 0},
    {S03_RAW(0x49D680), 0, {}, kG, 0, 0},
    {S03_RAW(0x49DBC0), 0, {}, kG, 0, 0},
    // BlitzBolt_Draw: its first word is not read, its second a byte
    {S03_RAW(0x49E720), 2, {0, kU8}, kG, 0, 0},
};
#undef S03_OURS
#undef S03_RAW

// The five .data handler tables the dispatchers read in place
// (MindSwordChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65A5E0, 3}, {0x65A5F4, 2}, {0x65A5FC, 3}, {0x65A62C, 1}, {0x65A630, 12},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850.., Scratch_Swap at +0xC
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
};

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: mh::Mem(kScratch + v % 16)[0] = Byte(h >> 24); break;
    case 3: {
        static const unsigned kFields[] = {0x10, 0x14, 0x16, 0x20, 0x2E, 0x30, 0x3E};
        SetWord(Sc() + kFields[v % 7], h >> 16);
        break;
    }
    case 4: mh::Mem(mh::at::kTarget)[0] ^= 0x40; break;
    case 5: {
        // an actor's state byte: the reaction state 6 or not
        unsigned char* const rec = v & 8 ? mh::EnemyOf(Byte(3 + v % 8)) : mh::PartyOf(Byte(v % 5));
        rec[1] = Byte(h & 0x1000000 ? 6 : h >> 24);
        break;
    }
    case 6: {
        static const unsigned kFields[] = {0x2E, 0x30, 0x34, 0x38, 0x3C};
        SetLong(mh::SpriteRecord(v) + kFields[(v >> 1) % 5], static_cast<std::int32_t>(h));
        break;
    }
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    g_k = k;
    Gfx_PacketNext = PrimAt(mh::Next());
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kMindSword_Task: case kBlitz_Task: case kChlorineChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kMindSwordChild_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kChlorine_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kBlitzBolt_Task: sc[1] = 0; break;
    case kMindSwordBlade_Run:
        sc[2] = Byte(mh::Next() % 6);
        sc[0xB] = Byte(mh::Half() ? 0 : mh::Half() ? 1 : sc[0xB]);   // the lead blade's draw, and the next
        break;
    case kMindSwordSpark_Run: case kChlorineCloud_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kMindSwordFlash_Run: case kChlorineCopy_Task: sc[2] = Byte(mh::Next() % 4); break;
    case kBlitzBolt_Run: sc[2] = Byte(mh::Next() % 12); break;
    // the counters: at their thresholds
    case kMindSwordBlade_Appear: case kMindSwordBlade_Spin: case kMindSwordBlade_Wait: case kMindSwordSpark_Start:
    case kChlorineCopy_Wait: case kBlitzBolt_Start: case kBlitzBolt_Bounce: case kBlitzBolt_Drift:
        Near(sc[9], 0);
        break;
    case kMindSwordSpark_Grow: Near(sc[9], 0x6F); break;
    case kMindSwordFlash_Grow: Near(sc[9], 0xB); break;
    case kChlorineCloud_Grow: Near(sc[9], 0xD); break;
    case kChlorineCloud_Start:
        Near(sc[9], 0);
        sc[0xB] = Byte(mh::Next() % 3);
        break;
    case kChlorineCopy_Play:
        if (mh::Half()) sc[9] = Byte(mh::Half() ? 0xFF : 1);
        break;
    // the waits on the children
    case kMindSwordBlade_Burst: case kMindSwordBlade_Fly: case kChlorine_Release: case kChlorine_WaitChildren:
    case kBlitz_End:
        if (mh::Half()) sc[0xB] = 0;
        if (k == kBlitz_End && mh::Half()) {
            const unsigned a = mh::Mem(mh::at::kActor)[0];
            unsigned char* const rec = a < 3 ? mh::PartyOf(Byte(a)) + 0x98 : mh::EnemyOf(Byte(a)) + 0xA4;
            SetWord(rec, mh::Next() % 4);
        }
        break;
    // the side Blitz aims at, the index its bolts keep, the reaction state
    case kBlitz_Start: case kBlitzBolt_Seek: case kBlitzBolt_Next:
        if (mh::Half()) mh::Mem(mh::at::kTarget)[0] |= 0x40;
        Settle();
        if (k == kBlitzBolt_Next && mh::Half()) {
            const unsigned i = sc[4];
            unsigned char* const rec = mh::Mem(mh::at::kTarget)[0] & 0x40
                                           ? mh::Mem(mh::at::kEnemies + i * mh::at::kEnemyStride)
                                           : mh::PartyOf(Byte(i));
            rec[1] = 6;
        }
        break;
    default: break;
    }
}

// BlitzBolt_Draw's two words: the blend its callers pass most of the time.
void Args(unsigned k, std::uint32_t* a) {
    if (k != kBlitzBolt_Draw || !mh::Often()) return;
    a[1] = mh::Next() % 2;
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s03", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    group.args = &Args;
    mh::Run(group);
    g_k = kCount;
}

}  // namespace magic_s03
