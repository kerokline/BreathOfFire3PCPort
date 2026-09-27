// BOF3X_SHADOW=magic_s34: group S34's five overlays (MAGIC158, 159, 161, 162,
// 166) through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s34.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC158 / 159 / 161 / 162 /
// 166 --clones (2026-09-27; capstone, every jump internal, no jump table, no
// REFUSED line), names given. Beyond the standard set this group lists the
// draw callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim, MapView_LinkPrimAt), the two sprite calls that act on
// Sprite_Current, and the functions of its own that its functions call
// directly. Everything the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - the GTE callees of the matrix push log what their pointers point at
//     (`deref`) and write a result where the real ones write (group S22's
//     effects, as S31 has them);
//   - the three pool allocators answer "none" (0xFF) or an index inside their
//     pool (kByte, wrapping through 0xFF), and MAGIC162's own calls log the
//     current record pointer 0x6AC808 (kPhase's masks[0], or an effect for
//     the two that take arguments);
//   - the group's disturbance moves the current record pointer, a byte of that
//     record, a pool record's live bit (the walks read each again after a
//     call), the scratch and vertex words, and the task's words the harness
//     leaves alone (+0x14, +0x20, +0x2E, +0x30, +0x3E).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s34.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s34 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC158 / 159 / 161 / 162 / 166 --clones,
// 2026-09-27, names given.
// 0x4ED670: 0x75 bytes  Charm_Task
constexpr mh::CallSite kCalls4ED670[] = {{0x53, 0x4ED7D0}};
constexpr mh::Imm kImms4ED670[] = {{0x16, 0x4ED6F0}, {0x1E, 0x4F7350}};
// 0x4ED6F0: 0xD4 bytes  Charm_Start
constexpr mh::CallSite kCalls4ED6F0[] = {{0x65, 0x4EDD70}, {0xA5, 0x5B93D2}, {0xC8, 0x587900}};
// 0x4ED7D0: 0x12 bytes; +0xB note: jmp through .data 0x65bf18, 1 code entries (a data_tables entry)  CharmMote_Task
// 0x4ED7F0: 0x33 bytes; +0xB note: call through .data 0x65bf1c, 3 code entries (a data_tables entry)  CharmMote_Run
constexpr mh::CallSite kCalls4ED7F0[] = {{0x23, 0x4EDA60}, {0x28, 0x4EDB30}, {0x2D, 0x5A7BC0}};
// 0x4ED830: 0x10F bytes  CharmMote_Launch
constexpr mh::CallSite kCalls4ED830[] = {{0x34, 0x5A7A00}, {0x5B, 0x5A7A50}, {0x94, 0x5B93D2}};
// 0x4ED940: 0x48 bytes  CharmMote_ToOwner
// 0x4ED990: 0xC5 bytes  CharmMote_Fall
constexpr mh::CallSite kCalls4ED990[] = {{0x20, 0x5A7A00}, {0x42, 0x5A7A50}};
// 0x4EDA60: 0xC8 bytes  CharmMote_PushMatrix
constexpr mh::CallSite kCalls4EDA60[] = {{0x3, 0x5A7B90}, {0x88, 0x5A8200}, {0x97, 0x5A8060}, {0xAB, 0x5A7D70}, {0xB5, 0x5A8DE0}, {0xBF, 0x5A8E00}};
// 0x4EDB30: 0x231 bytes  CharmMote_Draw
constexpr mh::CallSite kCalls4EDB30[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x9A, 0x5A75B0}, {0xA2, 0x5A7780}, {0xB8, 0x5A7A00}, {0xD1, 0x5A7A50}, {0xFE, 0x5A7A00}, {0x117, 0x5A7A50}, {0x15D, 0x5A7A00}, {0x17D, 0x5A7A50}, {0x1DF, 0x5A85F0}, {0x1E5, 0x5A9240}, {0x215, 0x572FA0}};
// 0x4EDD70: 0x57 bytes  Charm_PoolAlloc
// 0x4EDDD0: 0x26 bytes  Magic159_Task
constexpr mh::Imm kImms4EDDD0[] = {{0xF, 0x4EDE00}, {0x17, 0x4F7350}};
// 0x4EDE00: 0xBC bytes  Magic159_Start
constexpr mh::CallSite kCalls4EDE00[] = {{0x3F, 0x435180}, {0xA9, 0x452F70}, {0xB3, 0x587900}};
// 0x4EDEC0: 0x12 bytes; +0xB note: jmp through .data 0x65bf28, 1 code entries (a data_tables entry)  Magic159Child_Task
// 0x4EDEE0: 0x42 bytes; +0xB note: call through .data 0x65bf2c, 4 code entries (a data_tables entry)  Magic159Child_Run
constexpr mh::CallSite kCalls4EDEE0[] = {{0x23, 0x4FBD10}, {0x3C, 0x4EE030}};
// 0x4EDF30: 0x63 bytes  Magic159Child_Wait
// 0x4EDFA0: 0x2A bytes  Magic159Child_Grow
// 0x4EDFD0: 0x1C bytes  Magic159Child_Hold
// 0x4EDFF0: 0x35 bytes  Magic159Child_Fade
constexpr mh::CallSite kCalls4EDFF0[] = {{0x2F, 0x4351F0}};
// 0x4EE030: 0x2A8 bytes  Magic159Child_Draw
constexpr mh::CallSite kCalls4EE030[] = {{0x10, 0x5A77C0}, {0x26, 0x572FA0}, {0x5F, 0x5A7630}, {0x67, 0x5A7780}, {0x71, 0x5A7A50}, {0x9F, 0x5A7A00}, {0xCD, 0x5A7A50}, {0xFB, 0x5A7A00}, {0x12C, 0x5A7A50}, {0x15A, 0x5A7A00}, {0x188, 0x5A7A50}, {0x1B6, 0x5A7A00}, {0x1ED, 0x5A79A0}, {0x1FD, 0x5A79E0}, {0x29D, 0x572FA0}};
// 0x4EE2E0: 0x2E bytes  TimedBlow_Task
constexpr mh::Imm kImms4EE2E0[] = {{0xF, 0x4EE310}, {0x17, 0x4EE430}, {0x22, 0x43FE80}};
// 0x4EE310: 0x117 bytes  TimedBlow_Start
constexpr mh::CallSite kCalls4EE310[] = {{0x5C, 0x4FB830}, {0x65, 0x435180}};
// 0x4EE430: 0x29 bytes  TimedBlow_Wait
constexpr mh::CallSite kCalls4EE430[] = {{0x18, 0x4FB830}};
// 0x4EE460: 0x12 bytes; +0xB note: jmp through .data 0x65bf3c, 1 code entries (a data_tables entry)  TimedBlowCopy_Task
// 0x4EE480: 0x54 bytes  TimedBlowCopy_Run
constexpr mh::CallSite kCalls4EE480[] = {{0x4B, 0x588F20}};
constexpr mh::Imm kImms4EE480[] = {{0xF, 0x4ED5C0}, {0x17, 0x4EE4E0}, {0x22, 0x4EE520}, {0x2A, 0x4EE560}, {0x32, 0x4AEE90}};
// 0x4EE4E0: 0x3F bytes  TimedBlowCopy_Strike
constexpr mh::CallSite kCalls4EE4E0[] = {{0x0, 0x589410}, {0x22, 0x4FC030}, {0x2E, 0x4530D0}};
// 0x4EE520: 0x33 bytes  TimedBlowCopy_Flash
constexpr mh::CallSite kCalls4EE520[] = {{0x12, 0x587900}, {0x29, 0x4EE580}, {0x2E, 0x589410}};
// 0x4EE560: 0x1F bytes  BattleFx_ScriptToEnd
constexpr mh::CallSite kCalls4EE560[] = {{0x0, 0x589410}};
// 0x4EE580: 0x13D bytes  TimedBlow_DrawFlash
constexpr mh::CallSite kCalls4EE580[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x43, 0x5A7610}, {0x4B, 0x5A7780}, {0xA4, 0x461E50}, {0xB0, 0x5A7610}, {0xB8, 0x5A7780}, {0x111, 0x461E50}, {0x126, 0x5A77C0}, {0x12F, 0x461E50}};
// 0x4EE6C0: 0x81 bytes  Transfer_Task
constexpr mh::CallSite kCalls4EE6C0[] = {{0x69, 0x4EE8C0}};
constexpr mh::Imm kImms4EE6C0[] = {{0x15, 0x4EE750}, {0x1D, 0x4B1E70}, {0x25, 0x4B1ED0}, {0x2D, 0x4EE8A0}, {0x35, 0x4B8F50}, {0x3D, 0x4F7350}};
// 0x4EE750: 0x149 bytes  Transfer_Start
constexpr mh::CallSite kCalls4EE750[] = {{0x5B, 0x4FBD10}, {0x8A, 0x587900}, {0xAD, 0x4EF190}, {0xE6, 0x5B93D2}};
// 0x4EE8C0: 0x12 bytes; +0xB note: jmp through .data 0x65c034, 1 code entries (a data_tables entry)  TransferMote_Task
// 0x4EE8E0: 0x8F bytes  TransferMote_Run
constexpr mh::CallSite kCalls4EE8E0[] = {{0x3B, 0x4EEF60}, {0x59, 0x4EEBE0}, {0x83, 0x4EED70}};
constexpr mh::Imm kImms4EE8E0[] = {{0xF, 0x4EE970}, {0x17, 0x4EEAE0}, {0x22, 0x4EEB50}};
// 0x4EE970: 0x161 bytes  TransferMote_Start
constexpr mh::CallSite kCalls4EE970[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
// 0x4EEAE0: 0x6C bytes  TransferMote_Rise
constexpr mh::CallSite kCalls4EEAE0[] = {{0x21, 0x5A7A00}};
// 0x4EEB50: 0x85 bytes  TransferMote_Fade
constexpr mh::CallSite kCalls4EEB50[] = {{0x21, 0x5A7A00}, {0x7F, 0x4EF1E0}};
// 0x4EEBE0: 0x185 bytes  TransferMote_DrawRays
constexpr mh::CallSite kCalls4EEBE0[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
// 0x4EED70: 0x1ED bytes  TransferMote_DrawForks
constexpr mh::CallSite kCalls4EED70[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
// 0x4EEF60: 0x22F bytes  TransferMote_DrawStar
constexpr mh::CallSite kCalls4EEF60[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
// 0x4EF190: 0x4A bytes  Transfer_PoolAlloc
// 0x4EF1E0: 0x2F bytes  TransferMote_Free
// 0x4EF210: 0x75 bytes  Monopolize_Task
constexpr mh::CallSite kCalls4EF210[] = {{0x53, 0x4EF370}};
constexpr mh::Imm kImms4EF210[] = {{0x16, 0x4EF290}, {0x1E, 0x4F7350}};
// 0x4EF290: 0xD4 bytes  Monopolize_Start
constexpr mh::CallSite kCalls4EF290[] = {{0x65, 0x4EF5C0}, {0xA5, 0x5B93D2}, {0xC8, 0x587900}};
// 0x4EF370: 0x12 bytes; +0xB note: jmp through .data 0x65c050, 1 code entries (a data_tables entry)  MonopolizeMote_Task
// 0x4EF390: 0x33 bytes; +0xB note: call through .data 0x65c054, 2 code entries (a data_tables entry)  MonopolizeMote_Run
constexpr mh::CallSite kCalls4EF390[] = {{0x23, 0x4EDA60}, {0x28, 0x4EDB30}, {0x2D, 0x5A7BC0}};
// 0x4EF3D0: 0x109 bytes  MonopolizeMote_Launch
constexpr mh::CallSite kCalls4EF3D0[] = {{0x34, 0x5A7A00}, {0x5B, 0x5A7A50}, {0x8E, 0x5B93D2}};
// 0x4EF4E0: 0xD3 bytes  MonopolizeMote_Fall
constexpr mh::CallSite kCalls4EF4E0[] = {{0x20, 0x5A7A00}, {0x42, 0x5A7A50}};
// 0x4EF5C0: 0x57 bytes  Monopolize_PoolAlloc
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S34_CLONE(name, base, size, calls, ncalls, imms, nimms) \
    {#name, base, size, calls, ncalls, imms, nimms, nullptr, 0, reinterpret_cast<const void*>(&::name)}
const mh::Clone kClones[] = {
    S34_CLONE(Charm_Task, 0x4ED670, 0x75, kCalls4ED670, MH_N(kCalls4ED670), kImms4ED670, MH_N(kImms4ED670)),
    S34_CLONE(Charm_Start, 0x4ED6F0, 0xD4, kCalls4ED6F0, MH_N(kCalls4ED6F0), nullptr, 0),
    S34_CLONE(CharmMote_Task, 0x4ED7D0, 0x12, nullptr, 0, nullptr, 0),
    S34_CLONE(CharmMote_Run, 0x4ED7F0, 0x33, kCalls4ED7F0, MH_N(kCalls4ED7F0), nullptr, 0),
    S34_CLONE(CharmMote_Launch, 0x4ED830, 0x10F, kCalls4ED830, MH_N(kCalls4ED830), nullptr, 0),
    S34_CLONE(CharmMote_ToOwner, 0x4ED940, 0x48, nullptr, 0, nullptr, 0),
    S34_CLONE(CharmMote_Fall, 0x4ED990, 0xC5, kCalls4ED990, MH_N(kCalls4ED990), nullptr, 0),
    S34_CLONE(CharmMote_PushMatrix, 0x4EDA60, 0xC8, kCalls4EDA60, MH_N(kCalls4EDA60), nullptr, 0),
    S34_CLONE(CharmMote_Draw, 0x4EDB30, 0x231, kCalls4EDB30, MH_N(kCalls4EDB30), nullptr, 0),
    {"Charm_PoolAlloc", 0x4EDD70, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Charm_PoolAlloc), 0xFF},
    S34_CLONE(Magic159_Task, 0x4EDDD0, 0x26, nullptr, 0, kImms4EDDD0, MH_N(kImms4EDDD0)),
    S34_CLONE(Magic159_Start, 0x4EDE00, 0xBC, kCalls4EDE00, MH_N(kCalls4EDE00), nullptr, 0),
    S34_CLONE(Magic159Child_Task, 0x4EDEC0, 0x12, nullptr, 0, nullptr, 0),
    S34_CLONE(Magic159Child_Run, 0x4EDEE0, 0x42, kCalls4EDEE0, MH_N(kCalls4EDEE0), nullptr, 0),
    S34_CLONE(Magic159Child_Wait, 0x4EDF30, 0x63, nullptr, 0, nullptr, 0),
    S34_CLONE(Magic159Child_Grow, 0x4EDFA0, 0x2A, nullptr, 0, nullptr, 0),
    S34_CLONE(Magic159Child_Hold, 0x4EDFD0, 0x1C, nullptr, 0, nullptr, 0),
    S34_CLONE(Magic159Child_Fade, 0x4EDFF0, 0x35, kCalls4EDFF0, MH_N(kCalls4EDFF0), nullptr, 0),
    S34_CLONE(Magic159Child_Draw, 0x4EE030, 0x2A8, kCalls4EE030, MH_N(kCalls4EE030), nullptr, 0),
    S34_CLONE(TimedBlow_Task, 0x4EE2E0, 0x2E, nullptr, 0, kImms4EE2E0, MH_N(kImms4EE2E0)),
    S34_CLONE(TimedBlow_Start, 0x4EE310, 0x117, kCalls4EE310, MH_N(kCalls4EE310), nullptr, 0),
    S34_CLONE(TimedBlow_Wait, 0x4EE430, 0x29, kCalls4EE430, MH_N(kCalls4EE430), nullptr, 0),
    S34_CLONE(TimedBlowCopy_Task, 0x4EE460, 0x12, nullptr, 0, nullptr, 0),
    S34_CLONE(TimedBlowCopy_Run, 0x4EE480, 0x54, kCalls4EE480, MH_N(kCalls4EE480), kImms4EE480, MH_N(kImms4EE480)),
    S34_CLONE(TimedBlowCopy_Strike, 0x4EE4E0, 0x3F, kCalls4EE4E0, MH_N(kCalls4EE4E0), nullptr, 0),
    S34_CLONE(TimedBlowCopy_Flash, 0x4EE520, 0x33, kCalls4EE520, MH_N(kCalls4EE520), nullptr, 0),
    S34_CLONE(BattleFx_ScriptToEnd, 0x4EE560, 0x1F, kCalls4EE560, MH_N(kCalls4EE560), nullptr, 0),
    S34_CLONE(TimedBlow_DrawFlash, 0x4EE580, 0x13D, kCalls4EE580, MH_N(kCalls4EE580), nullptr, 0),
    S34_CLONE(Transfer_Task, 0x4EE6C0, 0x81, kCalls4EE6C0, MH_N(kCalls4EE6C0), kImms4EE6C0, MH_N(kImms4EE6C0)),
    S34_CLONE(Transfer_Start, 0x4EE750, 0x149, kCalls4EE750, MH_N(kCalls4EE750), nullptr, 0),
    S34_CLONE(TransferMote_Task, 0x4EE8C0, 0x12, nullptr, 0, nullptr, 0),
    S34_CLONE(TransferMote_Run, 0x4EE8E0, 0x8F, kCalls4EE8E0, MH_N(kCalls4EE8E0), kImms4EE8E0, MH_N(kImms4EE8E0)),
    S34_CLONE(TransferMote_Start, 0x4EE970, 0x161, kCalls4EE970, MH_N(kCalls4EE970), nullptr, 0),
    S34_CLONE(TransferMote_Rise, 0x4EEAE0, 0x6C, kCalls4EEAE0, MH_N(kCalls4EEAE0), nullptr, 0),
    S34_CLONE(TransferMote_Fade, 0x4EEB50, 0x85, kCalls4EEB50, MH_N(kCalls4EEB50), nullptr, 0),
    S34_CLONE(TransferMote_DrawRays, 0x4EEBE0, 0x185, kCalls4EEBE0, MH_N(kCalls4EEBE0), nullptr, 0),
    S34_CLONE(TransferMote_DrawForks, 0x4EED70, 0x1ED, kCalls4EED70, MH_N(kCalls4EED70), nullptr, 0),
    S34_CLONE(TransferMote_DrawStar, 0x4EEF60, 0x22F, kCalls4EEF60, MH_N(kCalls4EEF60), nullptr, 0),
    {"Transfer_PoolAlloc", 0x4EF190, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Transfer_PoolAlloc), 0xFF},
    S34_CLONE(TransferMote_Free, 0x4EF1E0, 0x2F, nullptr, 0, nullptr, 0),
    S34_CLONE(Monopolize_Task, 0x4EF210, 0x75, kCalls4EF210, MH_N(kCalls4EF210), kImms4EF210, MH_N(kImms4EF210)),
    S34_CLONE(Monopolize_Start, 0x4EF290, 0xD4, kCalls4EF290, MH_N(kCalls4EF290), nullptr, 0),
    S34_CLONE(MonopolizeMote_Task, 0x4EF370, 0x12, nullptr, 0, nullptr, 0),
    S34_CLONE(MonopolizeMote_Run, 0x4EF390, 0x33, kCalls4EF390, MH_N(kCalls4EF390), nullptr, 0),
    S34_CLONE(MonopolizeMote_Launch, 0x4EF3D0, 0x109, kCalls4EF3D0, MH_N(kCalls4EF3D0), nullptr, 0),
    S34_CLONE(MonopolizeMote_Fall, 0x4EF4E0, 0xD3, kCalls4EF4E0, MH_N(kCalls4EF4E0), nullptr, 0),
    {"Monopolize_PoolAlloc", 0x4EF5C0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Monopolize_PoolAlloc), 0xFF},
};
#undef S34_CLONE
#undef MH_N

enum : unsigned {
    kCharm_Task, kCharm_Start, kCharmMote_Task, kCharmMote_Run, kCharmMote_Launch, kCharmMote_ToOwner, kCharmMote_Fall,
    kCharmMote_PushMatrix, kCharmMote_Draw, kCharm_PoolAlloc,
    kMagic159_Task, kMagic159_Start, kMagic159Child_Task, kMagic159Child_Run, kMagic159Child_Wait, kMagic159Child_Grow,
    kMagic159Child_Hold, kMagic159Child_Fade, kMagic159Child_Draw,
    kTimedBlow_Task, kTimedBlow_Start, kTimedBlow_Wait, kTimedBlowCopy_Task, kTimedBlowCopy_Run, kTimedBlowCopy_Strike,
    kTimedBlowCopy_Flash, kBattleFx_ScriptToEnd, kTimedBlow_DrawFlash,
    kTransfer_Task, kTransfer_Start, kTransferMote_Task, kTransferMote_Run, kTransferMote_Start, kTransferMote_Rise,
    kTransferMote_Fade, kTransferMote_DrawRays, kTransferMote_DrawForks, kTransferMote_DrawStar, kTransfer_PoolAlloc,
    kTransferMote_Free,
    kMonopolize_Task, kMonopolize_Start, kMonopolizeMote_Task, kMonopolizeMote_Run, kMonopolizeMote_Launch,
    kMonopolizeMote_Fall, kMonopolize_PoolAlloc, kCount
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
constexpr std::uint32_t kCharmPool = 0x6A9108, kTransferPool = 0x6AB208, kTransferCurrent = 0x6AC808,
                        kMonopolizePool = 0x6AC810, kPoolsEnd = 0x6AE910;

unsigned char* MoteRecord(std::uint32_t pool, unsigned i) { return mh::Mem(pool + (i % 64) * 0x84u); }
unsigned char* TransferRecord(unsigned i) { return mh::Mem(kTransferPool + (i % 128) * 0x2Cu); }
unsigned char* Cur() { return mh::Pointer(kTransferCurrent); }

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer (a draw writes up to 0x58 past it).
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
// The sprite calls that act on Sprite_Current: which sprite.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
// MAGIC162's line draws: the current record they draw.
std::uint32_t NoteCurrent(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(static_cast<std::uint32_t>(Long(mh::Mem(kTransferCurrent))));
    return answer;
}
// The GTE stand-ins of the matrix push: a result from the inputs, written
// where the real callee writes (group S22's).
std::uint32_t RotTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* v = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* t = reinterpret_cast<long*>(static_cast<std::uintptr_t>(a[1]));
    t[0] = v[0] * 3 + 1;
    t[1] = v[1] * 5 - 2;
    t[2] = v[2] * 7 + 3;
    return answer;
}
std::uint32_t RotMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* r = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* m = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[1]));
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(r[i % 3] * static_cast<int>(i + 1) + static_cast<int>(i));
    return answer;
}
std::uint32_t MulMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* b = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[1]));
    auto* out = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[2]));
    mh::Note(a[1] == a[2], a[0]);
    for (unsigned i = 0; i < 9; ++i) out[i] = static_cast<short>(b[i] * 3 + 7);
    return answer;
}
std::uint32_t SetTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 0x14, 12);
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
constexpr mh::Answer kP = mh::Answer::kPhase;
#define S34_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S34_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S34_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S34_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSprite},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, battle_items:
    // all ours)
    {S34_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S34_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S34_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S34_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S34_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetPolyF4), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gpu_SetLineG3), 1, {kAll}, kG, 0, 0},
    {S34_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S34_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S34_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S34_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S34_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S34_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S34_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S34_OURS(Gte_PrimDepths4_0C), 1, {kAll}, kG, 0, 0},
    // this group's own, called directly: the pool allocators answer "none" or
    // an index inside their pool; MAGIC162's log the current record
    {S34_RAW(0x4EDD70), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
    {S34_RAW(0x4EF5C0), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
    {S34_RAW(0x4EF190), 0, {}, mh::Answer::kByte, 0xFF, 0x7F},
    {S34_RAW(0x4ED7D0), 0, {}, kP, 0, 0},
    {S34_RAW(0x4EF370), 0, {}, kP, 0, 0},
    {S34_RAW(0x4EDA60), 0, {}, kP, 0, 0},
    {S34_RAW(0x4EDB30), 0, {}, kP, 0, 0},
    {S34_RAW(0x4EE030), 0, {}, kP, 0, 0},
    {S34_RAW(0x4EE580), 0, {}, kP, 0, 0},
    {S34_RAW(0x4EE8C0), 0, {kTransferCurrent}, kP, 0, 0},
    {S34_RAW(0x4EEF60), 0, {kTransferCurrent}, kP, 0, 0},
    {S34_RAW(0x4EF1E0), 0, {kTransferCurrent}, kP, 0, 0},
    {S34_RAW(0x4EEBE0), 2, {kU16, kU16}, kG, 0, 0, {}, &NoteCurrent},
    {S34_RAW(0x4EED70), 2, {kU16, kU16}, kG, 0, 0, {}, &NoteCurrent},
};
#undef S34_OURS
#undef S34_RAW

// The eight .data handler tables the dispatchers read in place
// (CharmMote_TaskTable and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65BF18, 1}, {0x65BF1C, 3}, {0x65BF28, 1}, {0x65BF2C, 4},
    {0x65BF3C, 1}, {0x65C034, 1}, {0x65C050, 1}, {0x65C054, 2},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                          // Gfx_PacketNext
    {0, kPrimBytes},                        // g_prims (filled in at start-up)
    {kVertex, 0x20},                        // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                       // 0x903850..0x90385F
    {kCharmPool, kPoolsEnd - kCharmPool},   // Charm_Pool, Transfer_Pool, TransferMote_Current, Monopolize_Pool
    {0x80E980, 0x200},                      // Gfx_ClutStripSource row 26
    {0x812980, 0x200},                      // Gfx_ClutStrip row 26
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: mh::Mem(kScratch + v % 16)[0] = Byte(h >> 24); break;
    case 3: mh::SetPointer(kTransferCurrent, TransferRecord(v)); break;
    case 4: {
        static const unsigned kFields[] = {0, 2, 5, 6, 7, 0xC};
        Cur()[kFields[v % 6]] = Byte(h >> 24);
        break;
    }
    case 5: {
        unsigned char* const rec = (v & 0x100) ? TransferRecord(v) : MoteRecord((v & 0x80) ? kMonopolizePool : kCharmPool, v);
        rec[0] = Byte(rec[0] ^ 1);
        break;
    }
    case 6: {
        static const unsigned kFields[] = {0x14, 0x20, 0x2E, 0x30, 0x3E};
        SetWord(Sc() + kFields[v % 5], h >> 16);
        break;
    }
    case 7: mh::Mem(kScratch + 2 * (v % 8))[1] = Byte(h >> 24); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

// A pool's live bits for its allocator: all taken, all but one, or random.
void FillPool(std::uint32_t pool, unsigned count, unsigned stride) {
    const unsigned how = mh::Next() % 3;
    if (how == 2) return;
    const unsigned free = mh::Next() % count;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const rec = mh::Mem(pool + i * stride);
        rec[0] = Byte(rec[0] | 1);
        if (how == 1 && i == free) rec[0] = Byte(rec[0] & ~1u);
    }
}

// A walk makes each live record's owner field the owner, which the harness's
// disturbance writes through: each one a real slot.
void FixOwners(std::uint32_t pool, unsigned count, unsigned stride, unsigned owner_at) {
    for (unsigned i = 0; i < count; ++i) mh::SetPointer(pool + i * stride + owner_at, mh::TaskAt(mh::Next()));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kTransferCurrent, TransferRecord(mh::Next()));
    unsigned char* const cur = Cur();
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kCharm_Task:
        sc[1] = Byte(mh::Next() % 2);
        FixOwners(kCharmPool, 64, 0x84, 0x80);
        break;
    case kMonopolize_Task:
        sc[1] = Byte(mh::Next() % 2);
        FixOwners(kMonopolizePool, 64, 0x84, 0x80);
        break;
    case kMagic159_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kTimedBlow_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kTransfer_Task:
        sc[1] = Byte(mh::Next() % 6);
        FixOwners(kTransferPool, 128, 0x2C, 0x28);
        break;
    case kCharmMote_Task: case kMagic159Child_Task: case kTimedBlowCopy_Task: case kMonopolizeMote_Task: sc[1] = 0; break;
    case kTransferMote_Task: cur[1] = 0; break;
    case kCharmMote_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kMonopolizeMote_Run: sc[2] = Byte(mh::Next() % 2); break;
    case kMagic159Child_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kTimedBlowCopy_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kTransferMote_Run:
        cur[2] = Byte(mh::Next() % 3);
        if (mh::Half()) cur[0] = Byte(cur[0] | 1);
        break;
    // the counters: at their thresholds
    case kCharmMote_Launch: case kMonopolizeMote_Launch: case kMagic159Child_Wait: case kTimedBlowCopy_Strike:
        Near(sc[9], 1);
        break;
    case kCharmMote_ToOwner:
        Near(sc[9], 0xF);
        Near(sc[0xA], 0xF);
        if (mh::Often()) SetWord(sc + 0x3E, (Word(mh::Pointer(mh::at::kOwner) + 0x3E) + 0x60 + (mh::Half() ? 1u : 0u)) & 0xFFFF);
        break;
    case kCharmMote_Fall: Near(sc[0xA], 1); break;
    case kMonopolizeMote_Fall:
        if (mh::Often()) sc[9] = Byte(0xF + mh::Next() % 2);
        Near(sc[0xA], 1);
        break;
    case kMagic159Child_Grow: if (mh::Half()) sc[0xA] = Byte(0xD + mh::Next() % 2); break;
    case kMagic159Child_Hold: Near(sc[9], 0x1F); break;
    case kMagic159Child_Fade: if (mh::Half()) sc[0xA] = Byte(2 + mh::Next() % 2); break;
    case kTimedBlow_Wait: if (mh::Half()) sc[0xB] = 0; break;
    case kTimedBlowCopy_Flash: if (mh::Half()) sc[9] = Byte(mh::Half() ? 0x10 : 0xC); break;
    case kTransferMote_Start: Near(cur[5], 1); break;
    case kTransferMote_Rise: if (mh::Half()) cur[7] = Byte(cur[6] + 1); break;
    case kTransferMote_Fade:
        if (mh::Half()) Frame_Counter &= ~3u;
        Near(cur[5], 1);
        break;
    // the allocators: a full pool, one free, or random
    case kCharm_PoolAlloc: FillPool(kCharmPool, 64, 0x84); break;
    case kMonopolize_PoolAlloc: FillPool(kMonopolizePool, 64, 0x84); break;
    case kTransfer_PoolAlloc: FillPool(kTransferPool, 128, 0x2C); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s34", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    mh::Run(group);
}

}  // namespace magic_s34
