// BOF3X_SHADOW=magic_s09: group S09's overlays (MAGIC045, 046/047, 048, 050)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s09.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC045 / MAGIC046/MAGIC047 /
// MAGIC048 / MAGIC050 --clones (2026-09-26; capstone, every jump internal, no
// jump table, no REFUSED line), names given. Beyond the standard set this
// group lists the draw callees (the GTE and libgpu entry points, Math_Sin /
// Math_Cos / Math_Ratan2, Gfx_CommitPrim, MapView_LinkPrimAt), the sprite
// calls, AreaMap_Elevation, Battle_ActorIsOut, the engine's 0x446770 and its
// own functions called directly. What the harness lacks is built here, not in
// the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log the whole packet
//     buffer (every primitive of a draw is built in it) and move
//     Gfx_PacketNext on as the real ones do;
//   - the matrix pushes' GTE callees log what their pointers point at
//     (`deref`) and write a result where the real ones write (group S22's);
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair and
//     writes a new pair, so what the caller reads back is compared;
//   - the sprite calls that act on Sprite_Current log which sprite, the screen
//     update the frame-offset table 0x9039D8 too, and the two children's
//     steps are listed before their .data tables register them, so each logs
//     that table as well (the runs swap it round the call);
//   - Math_Sin / Math_Cos rewrite a scratch or vertex word a quarter of the
//     time (group S07's), so each re-read of one after a trig call is
//     compared;
//   - Battle_ActorIsOut leaves the last actor of DreamBreath_TargetHeight's
//     loop in when none is yet (the original's idiv faults on none: its
//     abort in ours is the precedent, not something to fuzz);
//   - DreamBreath_TargetHeight's stand-in writes the height it computes, so
//     DreamBreathMote_Start's read of it after the call is compared.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s09.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s09 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

#define S09_P(name) reinterpret_cast<const void*>(&::name)

// tools/magic_rows.py --unit MAGIC045 / MAGIC046/MAGIC047 / MAGIC048 /
// MAGIC050 --clones, 2026-09-26, names given.
// 0x4A9830: 0x26 bytes  BoneDart_Task
constexpr mh::Imm kImms4A9830[] = {{0xF, 0x4A9860}, {0x17, 0x4E5200}};
// 0x4A9860: 0x131 bytes  BoneDart_Start
constexpr mh::CallSite kCalls4A9860[] = {{0x1F, 0x4530D0}, {0x2E, 0x4351F0}, {0x7F, 0x435180}, {0xB5, 0x435180}};
// 0x4A99A0: 0x12 bytes; +0xB note: jmp through .data 0x65a948 (a data_tables entry)  BoneDartChild_Task
// 0x4A99C0: 0x3D bytes; +0x15 note: call through .data 0x65a950 (a data_tables entry)  BoneDartShaft_Run
constexpr mh::CallSite kCalls4A99C0[] = {{0x2D, 0x588F20}};
// 0x4A9A00: 0x12A bytes  BoneDartShaft_Start
constexpr mh::CallSite kCalls4A9A00[] = {{0x30, 0x446770}, {0x110, 0x5891F0}};
// 0x4A9B30: 0x12C bytes  BoneDartShaft_Fly
constexpr mh::CallSite kCalls4A9B30[] = {{0x4, 0x5893A0}, {0x48, 0x4FBA90}, {0x59, 0x4FBC30}, {0x77, 0x4530D0}, {0x81, 0x587900}, {0xEB, 0x5A7A70}};
// 0x4A9C60: 0xB0 bytes  BoneDartShaft_Bounce
constexpr mh::CallSite kCalls4A9C60[] = {{0x9, 0x5893A0}, {0x44, 0x5A7A00}, {0x61, 0x5A7A50}};
// 0x4A9D10: 0x3D bytes; +0x15 note: call through .data 0x65a960 (a data_tables entry)  BoneDartShadow_Run
constexpr mh::CallSite kCalls4A9D10[] = {{0x2D, 0x588F20}};
// 0x4A9D50: 0xF9 bytes  BoneDartShadow_Start
constexpr mh::CallSite kCalls4A9D50[] = {{0xDE, 0x5891F0}};
// 0x4A9E50: 0xA7 bytes  BoneDartShadow_Follow
constexpr mh::CallSite kCalls4A9E50[] = {{0x0, 0x5893A0}, {0x4A, 0x5720C0}};
// 0x4A9F00: 0xD9 bytes  BoneDartShadow_Fade
constexpr mh::CallSite kCalls4A9F00[] = {{0x9, 0x5893A0}, {0x7C, 0x5720C0}};
// 0x4A9FE0: 0x75 bytes  ElemBreath_Task
constexpr mh::CallSite kCalls4A9FE0[] = {{0x53, 0x4AA360}};
constexpr mh::Imm kImms4A9FE0[] = {{0x16, 0x4AA060}, {0x1E, 0x4F7350}};
// 0x4AA060: 0x139 bytes  ElemBreath_Start
constexpr mh::CallSite kCalls4AA060[] = {{0x66, 0x435180}, {0xB5, 0x435180}, {0x11E, 0x587900}};
// 0x4AA1A0: 0x12 bytes; +0xB note: jmp through .data 0x65a970 (a data_tables entry)  ElemBreathChild_Task
// 0x4AA1C0: 0x2E bytes  ElemBreathEmitter_Task
constexpr mh::Imm kImms4AA1C0[] = {{0xF, 0x4AA1F0}, {0x17, 0x4AA260}, {0x22, 0x4D1AA0}};
// 0x4AA1F0: 0x61 bytes  ElemBreathEmitter_Start
// 0x4AA260: 0x8D bytes  ElemBreathEmitter_Emit
constexpr mh::CallSite kCalls4AA260[] = {{0xB, 0x4AABE0}};
// 0x4AA2F0: 0x2E bytes  ElemBreathEnemy_Task
constexpr mh::Imm kImms4AA2F0[] = {{0xF, 0x4AA320}, {0x17, 0x43EC10}, {0x22, 0x4AEE90}};
// 0x4AA320: 0x36 bytes  ElemBreathEnemy_Start
constexpr mh::CallSite kCalls4AA320[] = {{0x19, 0x5891F0}};
// 0x4AA360: 0x48 bytes  ElemBreathMote_Run
constexpr mh::CallSite kCalls4AA360[] = {{0x35, 0x4AA980}, {0x3A, 0x4AAA30}, {0x3F, 0x5A7BC0}};
constexpr mh::Imm kImms4AA360[] = {{0xF, 0x4AA3B0}, {0x17, 0x4AA510}, {0x22, 0x4AA5D0}};
// 0x4AA3B0: 0x156 bytes  ElemBreathMote_Start
constexpr mh::CallSite kCalls4AA3B0[] = {{0x2, 0x4FC0E0}, {0x3A, 0x5A7A70}, {0xEE, 0x446770}};
// 0x4AA510: 0xB8 bytes  ElemBreathMote_Flow
constexpr mh::CallSite kCalls4AA510[] = {{0x21, 0x5A7A00}, {0x4E, 0x5A7A00}, {0x74, 0x5A7A50}, {0x91, 0x4FBD10}, {0x96, 0x4AA6D0}};
// 0x4AA5D0: 0xFA bytes  ElemBreathMote_Fade
constexpr mh::CallSite kCalls4AA5D0[] = {{0x21, 0x5A7A00}, {0x4E, 0x5A7A00}, {0x74, 0x5A7A50}, {0x91, 0x4FBD10}, {0x96, 0x4AA6D0}};
// 0x4AA6D0: 0x2AE bytes  ElemBreathMote_Draw
constexpr mh::CallSite kCalls4AA6D0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x37, 0x5A7630}, {0x3E, 0x5A7780}, {0x9C, 0x5A7A00}, {0xC7, 0x5A7A50}, {0xF2, 0x5A7A00}, {0x11D, 0x5A7A50}, {0x145, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x198, 0x5A7A00}, {0x1C3, 0x5A7A50}, {0x1F6, 0x5A79A0}, {0x206, 0x5A79E0}, {0x2A2, 0x572FA0}};
// 0x4AA980: 0xA4 bytes  ElemBreathMote_PushMatrix
constexpr mh::CallSite kCalls4AA980[] = {{0x3, 0x5A7B90}, {0x64, 0x5A8200}, {0x73, 0x5A8060}, {0x87, 0x5A7D70}, {0x91, 0x5A8DE0}, {0x9B, 0x5A8E00}};
// 0x4AAA30: 0x1AC bytes  ElemBreathMote_DrawFlare
constexpr mh::CallSite kCalls4AAA30[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x52, 0x5A7A00}, {0x6B, 0x5A7A50}, {0x92, 0x5A75F0}, {0x99, 0x5A7780}, {0xC7, 0x5A7A00}, {0xE0, 0x5A7A50}, {0x132, 0x5A84A0}, {0x138, 0x5A9310}, {0x16D, 0x461E50}, {0x193, 0x5A77C0}, {0x19C, 0x461E50}};
// 0x4AABE0: 0x57 bytes  ElemBreathMote_Alloc
// 0x4AAC40: 0x2E bytes  DreamBreath_Task
constexpr mh::Imm kImms4AAC40[] = {{0xF, 0x4AAC70}, {0x17, 0x4F7320}, {0x22, 0x4F7350}};
// 0x4AAC70: 0xD1 bytes  DreamBreath_Start
constexpr mh::CallSite kCalls4AAC70[] = {{0x6C, 0x435180}};
// 0x4AAD50: 0x12 bytes; +0xB note: jmp through .data 0x65a978 (a data_tables entry)  DreamBreathMote_Task
// 0x4AAD70: 0x33 bytes; +0xB note: call through .data 0x65a97c (a data_tables entry)  DreamBreathMote_Run
constexpr mh::CallSite kCalls4AAD70[] = {{0x23, 0x4AB250}, {0x28, 0x4AAFD0}, {0x2D, 0x5A7BC0}};
// 0x4AADB0: 0xFC bytes  DreamBreathMote_Start
constexpr mh::CallSite kCalls4AADB0[] = {{0x1E, 0x4AB330}, {0x3F, 0x446770}, {0x89, 0x446770}};
// 0x4AAEB0: 0x93 bytes  DreamBreathMote_Fly
constexpr mh::CallSite kCalls4AAEB0[] = {{0x49, 0x4FBA90}, {0x7A, 0x4FBC70}};
// 0x4AAF50: 0x7D bytes  DreamBreathMote_Land
constexpr mh::CallSite kCalls4AAF50[] = {{0x3C, 0x4FBA90}, {0x73, 0x4351F0}};
// 0x4AAFD0: 0x27C bytes  DreamBreathMote_Draw
constexpr mh::CallSite kCalls4AAFD0[] = {{0x18, 0x5A77C0}, {0x2E, 0x572FA0}, {0x63, 0x5B93D2}, {0xA4, 0x5A7A50}, {0xBD, 0x5A7A00}, {0xE4, 0x5A7A50}, {0xFD, 0x5A7A00}, {0x124, 0x5A7610}, {0x12B, 0x5A7780}, {0x14B, 0x5A7A50}, {0x164, 0x5A7A00}, {0x197, 0x5A7A50}, {0x1B0, 0x5A7A00}, {0x23C, 0x5A85F0}, {0x245, 0x5A9350}, {0x25B, 0x572FA0}};
// 0x4AB250: 0xD4 bytes  DreamBreathMote_PushMatrix
constexpr mh::CallSite kCalls4AB250[] = {{0x3, 0x5A7B90}, {0x3F, 0x5A7A70}, {0x94, 0x5A8200}, {0xA3, 0x5A8060}, {0xB7, 0x5A7D70}, {0xC1, 0x5A8DE0}, {0xCB, 0x5A8E00}};
// 0x4AB330: 0xBC bytes  DreamBreath_TargetHeight
constexpr mh::CallSite kCalls4AB330[] = {{0x24, 0x4456C0}, {0x69, 0x4456C0}};
// 0x4AB3F0: 0x2E bytes  Pollen_Task
constexpr mh::Imm kImms4AB3F0[] = {{0xF, 0x4AB420}, {0x17, 0x4AB520}, {0x22, 0x4F7350}};
// 0x4AB420: 0xF8 bytes  Pollen_Start
constexpr mh::CallSite kCalls4AB420[] = {{0xC, 0x4FC0E0}, {0x70, 0x435180}, {0xEC, 0x587900}};
// 0x4AB520: 0x44 bytes  Pollen_Sounds
constexpr mh::CallSite kCalls4AB520[] = {{0x19, 0x587900}};
// 0x4AB570: 0x12 bytes; +0xB note: jmp through .data 0x65a994 (a data_tables entry)  PollenMote_Task
// 0x4AB590: 0x38 bytes; +0xB note: call through .data 0x65a9e8 (a data_tables entry)  PollenMote_Run
constexpr mh::CallSite kCalls4AB590[] = {{0x23, 0x4FBD10}, {0x28, 0x4AB6C0}, {0x2D, 0x4AB8E0}, {0x32, 0x4ABB70}};
// 0x4AB5D0: 0xAD bytes  PollenMote_Start
constexpr mh::CallSite kCalls4AB5D0[] = {{0x46, 0x446770}, {0x89, 0x452F70}};
// 0x4AB680: 0x34 bytes  PollenMote_Fade
constexpr mh::CallSite kCalls4AB680[] = {{0x2E, 0x4351F0}};
// 0x4AB6C0: 0x213 bytes  PollenMote_DrawFan
constexpr mh::CallSite kCalls4AB6C0[] = {{0x56, 0x5A77C0}, {0x6C, 0x572FA0}, {0x78, 0x5A75F0}, {0x7F, 0x5A7780}, {0xAE, 0x5A7A00}, {0xD5, 0x5A7A50}, {0x102, 0x5A7A00}, {0x129, 0x5A7A50}, {0x1FA, 0x572FA0}};
// 0x4AB8E0: 0x28B bytes  PollenMote_DrawRing
constexpr mh::CallSite kCalls4AB8E0[] = {{0x69, 0x5A77C0}, {0x7F, 0x572FA0}, {0x8B, 0x5A7610}, {0x92, 0x5A7780}, {0x98, 0x5A7A00}, {0xBF, 0x5A7A50}, {0xEC, 0x5A7A00}, {0x113, 0x5A7A50}, {0x13D, 0x5A7A00}, {0x164, 0x5A7A50}, {0x18B, 0x5A7A00}, {0x1B2, 0x5A7A50}, {0x26F, 0x572FA0}};
// 0x4ABB70: 0x126 bytes  PollenMote_DrawSparks
constexpr mh::CallSite kCalls4ABB70[] = {{0x42, 0x5A77C0}, {0x58, 0x572FA0}, {0x64, 0x5A7750}, {0x6C, 0x5A7780}, {0x72, 0x5A7A00}, {0x99, 0x5A7A50}, {0x108, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"BoneDart_Task", 0x4A9830, 0x26, nullptr, 0, kImms4A9830, MH_N(kImms4A9830), nullptr, 0, S09_P(BoneDart_Task)},
    {"BoneDart_Start", 0x4A9860, 0x131, kCalls4A9860, MH_N(kCalls4A9860), nullptr, 0, nullptr, 0, S09_P(BoneDart_Start)},
    {"BoneDartChild_Task", 0x4A99A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S09_P(BoneDartChild_Task)},
    {"BoneDartShaft_Run", 0x4A99C0, 0x3D, kCalls4A99C0, MH_N(kCalls4A99C0), nullptr, 0, nullptr, 0, S09_P(BoneDartShaft_Run)},
    {"BoneDartShaft_Start", 0x4A9A00, 0x12A, kCalls4A9A00, MH_N(kCalls4A9A00), nullptr, 0, nullptr, 0, S09_P(BoneDartShaft_Start)},
    {"BoneDartShaft_Fly", 0x4A9B30, 0x12C, kCalls4A9B30, MH_N(kCalls4A9B30), nullptr, 0, nullptr, 0, S09_P(BoneDartShaft_Fly)},
    {"BoneDartShaft_Bounce", 0x4A9C60, 0xB0, kCalls4A9C60, MH_N(kCalls4A9C60), nullptr, 0, nullptr, 0, S09_P(BoneDartShaft_Bounce)},
    {"BoneDartShadow_Run", 0x4A9D10, 0x3D, kCalls4A9D10, MH_N(kCalls4A9D10), nullptr, 0, nullptr, 0, S09_P(BoneDartShadow_Run)},
    {"BoneDartShadow_Start", 0x4A9D50, 0xF9, kCalls4A9D50, MH_N(kCalls4A9D50), nullptr, 0, nullptr, 0, S09_P(BoneDartShadow_Start)},
    {"BoneDartShadow_Follow", 0x4A9E50, 0xA7, kCalls4A9E50, MH_N(kCalls4A9E50), nullptr, 0, nullptr, 0, S09_P(BoneDartShadow_Follow)},
    {"BoneDartShadow_Fade", 0x4A9F00, 0xD9, kCalls4A9F00, MH_N(kCalls4A9F00), nullptr, 0, nullptr, 0, S09_P(BoneDartShadow_Fade)},
    {"ElemBreath_Task", 0x4A9FE0, 0x75, kCalls4A9FE0, MH_N(kCalls4A9FE0), kImms4A9FE0, MH_N(kImms4A9FE0), nullptr, 0, S09_P(ElemBreath_Task)},
    {"ElemBreath_Start", 0x4AA060, 0x139, kCalls4AA060, MH_N(kCalls4AA060), nullptr, 0, nullptr, 0, S09_P(ElemBreath_Start)},
    {"ElemBreathChild_Task", 0x4AA1A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S09_P(ElemBreathChild_Task)},
    {"ElemBreathEmitter_Task", 0x4AA1C0, 0x2E, nullptr, 0, kImms4AA1C0, MH_N(kImms4AA1C0), nullptr, 0, S09_P(ElemBreathEmitter_Task)},
    {"ElemBreathEmitter_Start", 0x4AA1F0, 0x61, nullptr, 0, nullptr, 0, nullptr, 0, S09_P(ElemBreathEmitter_Start)},
    {"ElemBreathEmitter_Emit", 0x4AA260, 0x8D, kCalls4AA260, MH_N(kCalls4AA260), nullptr, 0, nullptr, 0, S09_P(ElemBreathEmitter_Emit)},
    {"ElemBreathEnemy_Task", 0x4AA2F0, 0x2E, nullptr, 0, kImms4AA2F0, MH_N(kImms4AA2F0), nullptr, 0, S09_P(ElemBreathEnemy_Task)},
    {"ElemBreathEnemy_Start", 0x4AA320, 0x36, kCalls4AA320, MH_N(kCalls4AA320), nullptr, 0, nullptr, 0, S09_P(ElemBreathEnemy_Start)},
    {"ElemBreathMote_Run", 0x4AA360, 0x48, kCalls4AA360, MH_N(kCalls4AA360), kImms4AA360, MH_N(kImms4AA360), nullptr, 0, S09_P(ElemBreathMote_Run)},
    {"ElemBreathMote_Start", 0x4AA3B0, 0x156, kCalls4AA3B0, MH_N(kCalls4AA3B0), nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_Start)},
    {"ElemBreathMote_Flow", 0x4AA510, 0xB8, kCalls4AA510, MH_N(kCalls4AA510), nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_Flow)},
    {"ElemBreathMote_Fade", 0x4AA5D0, 0xFA, kCalls4AA5D0, MH_N(kCalls4AA5D0), nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_Fade)},
    {"ElemBreathMote_Draw", 0x4AA6D0, 0x2AE, kCalls4AA6D0, MH_N(kCalls4AA6D0), nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_Draw)},
    {"ElemBreathMote_PushMatrix", 0x4AA980, 0xA4, kCalls4AA980, MH_N(kCalls4AA980), nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_PushMatrix)},
    {"ElemBreathMote_DrawFlare", 0x4AAA30, 0x1AC, kCalls4AAA30, MH_N(kCalls4AAA30), nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_DrawFlare)},
    {"ElemBreathMote_Alloc", 0x4AABE0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, S09_P(ElemBreathMote_Alloc), 0xFF},
    {"DreamBreath_Task", 0x4AAC40, 0x2E, nullptr, 0, kImms4AAC40, MH_N(kImms4AAC40), nullptr, 0, S09_P(DreamBreath_Task)},
    {"DreamBreath_Start", 0x4AAC70, 0xD1, kCalls4AAC70, MH_N(kCalls4AAC70), nullptr, 0, nullptr, 0, S09_P(DreamBreath_Start)},
    {"DreamBreathMote_Task", 0x4AAD50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_Task)},
    {"DreamBreathMote_Run", 0x4AAD70, 0x33, kCalls4AAD70, MH_N(kCalls4AAD70), nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_Run)},
    {"DreamBreathMote_Start", 0x4AADB0, 0xFC, kCalls4AADB0, MH_N(kCalls4AADB0), nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_Start)},
    {"DreamBreathMote_Fly", 0x4AAEB0, 0x93, kCalls4AAEB0, MH_N(kCalls4AAEB0), nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_Fly)},
    {"DreamBreathMote_Land", 0x4AAF50, 0x7D, kCalls4AAF50, MH_N(kCalls4AAF50), nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_Land)},
    {"DreamBreathMote_Draw", 0x4AAFD0, 0x27C, kCalls4AAFD0, MH_N(kCalls4AAFD0), nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_Draw)},
    {"DreamBreathMote_PushMatrix", 0x4AB250, 0xD4, kCalls4AB250, MH_N(kCalls4AB250), nullptr, 0, nullptr, 0, S09_P(DreamBreathMote_PushMatrix)},
    {"DreamBreath_TargetHeight", 0x4AB330, 0xBC, kCalls4AB330, MH_N(kCalls4AB330), nullptr, 0, nullptr, 0, S09_P(DreamBreath_TargetHeight)},
    {"Pollen_Task", 0x4AB3F0, 0x2E, nullptr, 0, kImms4AB3F0, MH_N(kImms4AB3F0), nullptr, 0, S09_P(Pollen_Task)},
    {"Pollen_Start", 0x4AB420, 0xF8, kCalls4AB420, MH_N(kCalls4AB420), nullptr, 0, nullptr, 0, S09_P(Pollen_Start)},
    {"Pollen_Sounds", 0x4AB520, 0x44, kCalls4AB520, MH_N(kCalls4AB520), nullptr, 0, nullptr, 0, S09_P(Pollen_Sounds)},
    {"PollenMote_Task", 0x4AB570, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S09_P(PollenMote_Task)},
    {"PollenMote_Run", 0x4AB590, 0x38, kCalls4AB590, MH_N(kCalls4AB590), nullptr, 0, nullptr, 0, S09_P(PollenMote_Run)},
    {"PollenMote_Start", 0x4AB5D0, 0xAD, kCalls4AB5D0, MH_N(kCalls4AB5D0), nullptr, 0, nullptr, 0, S09_P(PollenMote_Start)},
    {"PollenMote_Fade", 0x4AB680, 0x34, kCalls4AB680, MH_N(kCalls4AB680), nullptr, 0, nullptr, 0, S09_P(PollenMote_Fade)},
    {"PollenMote_DrawFan", 0x4AB6C0, 0x213, kCalls4AB6C0, MH_N(kCalls4AB6C0), nullptr, 0, nullptr, 0, S09_P(PollenMote_DrawFan)},
    {"PollenMote_DrawRing", 0x4AB8E0, 0x28B, kCalls4AB8E0, MH_N(kCalls4AB8E0), nullptr, 0, nullptr, 0, S09_P(PollenMote_DrawRing)},
    {"PollenMote_DrawSparks", 0x4ABB70, 0x126, kCalls4ABB70, MH_N(kCalls4ABB70), nullptr, 0, nullptr, 0, S09_P(PollenMote_DrawSparks)},
};
#undef MH_N

enum : unsigned {
    kBoneDart_Task, kBoneDart_Start, kBoneDartChild_Task, kBoneDartShaft_Run, kBoneDartShaft_Start, kBoneDartShaft_Fly,
    kBoneDartShaft_Bounce, kBoneDartShadow_Run, kBoneDartShadow_Start, kBoneDartShadow_Follow, kBoneDartShadow_Fade,
    kElemBreath_Task, kElemBreath_Start, kElemBreathChild_Task, kElemBreathEmitter_Task, kElemBreathEmitter_Start,
    kElemBreathEmitter_Emit, kElemBreathEnemy_Task, kElemBreathEnemy_Start, kElemBreathMote_Run, kElemBreathMote_Start,
    kElemBreathMote_Flow, kElemBreathMote_Fade, kElemBreathMote_Draw, kElemBreathMote_PushMatrix,
    kElemBreathMote_DrawFlare, kElemBreathMote_Alloc,
    kDreamBreath_Task, kDreamBreath_Start, kDreamBreathMote_Task, kDreamBreathMote_Run, kDreamBreathMote_Start,
    kDreamBreathMote_Fly, kDreamBreathMote_Land, kDreamBreathMote_Draw, kDreamBreathMote_PushMatrix,
    kDreamBreath_TargetHeight,
    kPollen_Task, kPollen_Start, kPollen_Sounds, kPollenMote_Task, kPollenMote_Run, kPollenMote_Start, kPollenMote_Fade,
    kPollenMote_DrawFan, kPollenMote_DrawRing, kPollenMote_DrawSparks, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into every round. The commits log it
// whole and move the pointer on by the size, as the real ones do; so every
// primitive of a draw is compared, and a copy that reads the pointer once
// where the original reads it again lands elsewhere.
constexpr unsigned kPacketBytes = 0x800;
alignas(16) unsigned char g_packets[kPacketBytes];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 7) * 0x40; }

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0, kFrameSet = 0x9039D8, kAbilityId = 0x904B80,
                        kEventBattle = 0x904AAA, kCurrentEnemy = 0x939AD8, kKind2 = 0x905E60,
                        kMotePool = 0x67DE40, kPoolBytes = 48 * 0x84, kPoolRecord255 = 0x67DE40 + 255 * 0x84;

// Set by the seed for the functions they matter to.
bool g_keep_slots = false;    // the four slots' +0xB stay below 155 (BoneDartShadow_Follow / _Fade)

// --- the callees' effects (after the recorder's log and disturbance) ------------

void Advance(std::uint32_t size) {
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_packets || p + 0x100 > g_packets + kPacketBytes) p = g_packets + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    Advance(a[3]);
    return answer;
}
// The sprite calls that act on Sprite_Current: which sprite; the screen
// update the frame-offset table too.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}
// Math_Sin / Math_Cos: a quarter of the time a scratch or vertex word
// rewritten from the stream (group S07's).
std::uint32_t TrigEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 != 0) return answer;
    const unsigned w = (h >> 4) % 24;
    const auto v = static_cast<std::uint16_t>(h >> 16);
    if (w < 8) SetWord(mh::Mem(kScratch + w * 2), v);
    else SetWord(mh::Mem(kVertices + (w - 8) * 2), v);
    return answer;
}
// Battle_ActorIsOut: DreamBreath_TargetHeight's last actor (10 of the
// enemies, 2 of the party) in when none is in yet (dword 0x903854 0), so the
// original's idiv never faults.
std::uint32_t ActorIsOutEffect(const std::uint32_t* a, std::uint32_t answer) {
    const unsigned actor = a[0] & 0xFF;
    if ((actor == 10 || actor == 2) && Long(mh::Mem(kScratch + 4)) == 0) return answer & 0xFFFFFF00u;
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
// AreaMap_Elevation: a third of the time the height of the task slot +0xB
// of Sprite_Current (BoneDartShadow_Follow / _Fade's dart, when inside the
// table), so the ground and the dart's height meet and the signed compare's
// equal case is reached (control B54).
std::uint32_t ElevationEffect(const std::uint32_t*, std::uint32_t answer) {
    const unsigned slot = Sprite_Current[0xB];
    if (slot >= 48 || mh::Noise() % 3 != 0) return answer;
    return (answer & 0xFFFF0000u) | move_script::Word(mh::Mem(mh::at::kTasks + slot * mh::at::kTaskStride + 0x3E));
}
// DreamBreath_TargetHeight writes Sprite_Current's height; its stand-in too.
std::uint32_t HeightEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::FillBytes(Sprite_Current + 0x3E, 2);
    return answer;
}
// The GTE stand-ins of the matrix pushes: a result from the inputs, written
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
    mh::Note(a[1] == a[2]);
    for (unsigned i = 0; i < 9; ++i) out[i] = static_cast<short>(b[i] * 3 + 7);
    return answer;
}
std::uint32_t SetTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 0x14, 12);
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kPh = mh::Answer::kPhase;
#define S09_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S09_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S09_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S09_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &ActorIsOutEffect},
    // the sprite calls
    {S09_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S09_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S09_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &ElevationEffect},
    // the draw library
    {S09_OURS(Math_Sin), 1, {kAll}, kG, 0, 0, {}, &TrigEffect},
    {S09_OURS(Math_Cos), 1, {kAll}, kG, 0, 0, {}, &TrigEffect},
    {S09_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    {S09_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S09_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S09_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S09_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S09_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S09_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S09_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S09_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S09_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    {S09_OURS(Gpu_SetTile1), 1, {kAll}, kG, 0, 0},
    {S09_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S09_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S09_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S09_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S09_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S09_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // The projections: the vertices logged by what they hold (six bytes of
    // each SVECTOR), the outputs in the packet buffer by address, the depth
    // and flag pointers into the caller's frame masked off.
    {S09_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S09_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S09_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S09_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S09_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // This group's own, called directly (the pool walk's ElemBreathMote_Run
    // logs which record and owner it ran for).
    {S09_RAW(0x4AA360), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AA6D0), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AA980), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AAA30), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AAFD0), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AB250), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AB330), 0, {}, kG, 0, 0, {}, &HeightEffect},
    {S09_RAW(0x4AB6C0), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4AB8E0), 0, {}, kPh, 0, 0},
    {S09_RAW(0x4ABB70), 0, {}, kPh, 0, 0},
    // the mote pool's alloc: an index 0..0x2F, or 0xFF (none free; its caller
    // writes record 255 regardless, 0x6861BC - a region below)
    {S09_RAW(0x4AABE0), 0, {}, mh::Answer::kByte, 0xFF, 0x2F},
    // The children's steps, called with the frame-offset table swapped:
    // listed before their .data tables register them as plain handlers.
    {S09_RAW(0x4A9A00), 0, {kFrameSet}, kPh, 0, 0},
    {S09_RAW(0x4A9B30), 0, {kFrameSet}, kPh, 0, 0},
    {S09_RAW(0x4A9C60), 0, {kFrameSet}, kPh, 0, 0},
    {S09_RAW(0x4A9D50), 0, {kFrameSet}, kPh, 0, 0},
    {S09_RAW(0x4A9E50), 0, {kFrameSet}, kPh, 0, 0},
    {S09_RAW(0x4A9F00), 0, {kFrameSet}, kPh, 0, 0},
    {S09_RAW(0x4AF490), 0, {kFrameSet}, kPh, 0, 0},
};
#undef S09_OURS
#undef S09_RAW

// The eight .data handler tables the dispatchers read in place (symbols.toml).
const mh::DataTable kTables[] = {
    {0x65A948, 2}, {0x65A950, 4}, {0x65A960, 4}, {0x65A970, 2},
    {0x65A978, 1}, {0x65A97C, 3}, {0x65A994, 1}, {0x65A9E8, 3},
};

mh::Region g_regions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {0x7E0670, 4},                               // Gfx_PacketNext
    {0, kPacketBytes},                           // g_packets (filled in at start-up)
    {0x812980, 0x40},                            // Gfx_ClutStrip 0x1A00..0x1A1F (a word past the head is compared)
    {0x80E980, 0x40},                            // Gfx_ClutStripSource, the same
    {kFrameSet, 4},                              // the frame-offset table pointer
    {kAbilityId, 4},                             // the ability word 0x904B80
    {kCurrentEnemy, 4},                          // the current enemy pointer
    {kKind2, 8},                                 // Field_Kind2Z, Field_Kind2X
    {kMotePool, kPoolBytes},                     // ElemBreath_Motes
    {kPoolRecord255, 0x84},                      // "record 255": an unchecked 0xFF alloc's writes
};

unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(unsigned i) { return mh::Mem(kMotePool + (i % 48) * 0x84u); }
unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }

// A real slot or record for a pool record's owner (the walk makes it the
// owner cell, which the recorders write through).
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// The group's cells a function reads again after a call.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 10) {
    case 0: Gfx_PacketNext = PacketAt(v); break;
    case 1: {
        // a scratch word; dword 0x903854 (DreamBreath_TargetHeight's count,
        // its divisor) only 1..8
        const unsigned k = v % 8;
        if (k == 2 || k == 3) SetLong(mh::Mem(kScratch + 4), static_cast<std::int32_t>(1 + v % 8));
        else SetWord(mh::Mem(kScratch + k * 2), word);
        break;
    }
    case 2: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 3: PoolRecord(v)[0] ^= 1; break;
    case 4: mh::Pointer(mh::at::kOwner)[0xB] = Byte(v % 3); break;
    case 5: SetWord(mh::Mem(kAbilityId), (v & 1) ? 0x32 : word); break;
    case 6: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 7:
        if (v & 1) mh::Mem(kEventBattle)[0] = Byte(v >> 1);
        else mh::Pointer(kCurrentEnemy)[0x100] = (v & 2) ? 0x29 : Byte(v >> 2);
        break;
    case 8: SetLong(mh::Mem(kKind2 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 9: mh::SetPointer(kMotePool + (v % 48) * 0x84u + 0x80, SomeOwner(v >> 6)); break;
    default: break;
    }
}

// BoneDartShadow_Follow / _Fade index the task slots by +0xB: a disturbance
// that leaves 155 or above there would fault both sides (past the image), so
// such a byte goes back inside the table.
void Settle() {
    if (!g_keep_slots) return;
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const t = mh::TaskAt(k);
        if (t[0xB] >= 155) t[0xB] = Byte(t[0xB] % 48);
    }
}

// --- the seed --------------------------------------------------------------------

// A byte `below` under `at` .. `above` over it.
unsigned char Near(unsigned at, unsigned below, unsigned above) {
    return Byte(at - below + mh::Next() % (below + above + 1));
}

void FillPool() {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % 49;
    for (unsigned i = 0; i < 48; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (mode == 0) rec[0] = Byte(rec[0] | 1);   // full
        else if (mode == 1) rec[0] = Byte(i < taken ? rec[0] | 1 : rec[0] & ~1u);
        mh::SetPointer(kMotePool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PacketAt(mh::Next());
    FillPool();
    g_keep_slots = k == kBoneDartShadow_Follow || k == kBoneDartShadow_Fade;
    // the current enemy: an enemy record or a sprite record, its +0x100 0x29
    // two times in three; the event-battle byte set two times in three
    mh::SetPointer(kCurrentEnemy, mh::Half() ? static_cast<const void*>(mh::EnemyOf(Byte(3 + mh::Next() % 8)))
                                             : static_cast<const void*>(mh::SpriteRecord(mh::Next())));
    if (mh::Often()) mh::Pointer(kCurrentEnemy)[0x100] = 0x29;
    else if (mh::Half()) mh::Pointer(kCurrentEnemy)[0x100] = Near(0x29, 1, 1);
    mh::Mem(kEventBattle)[0] = mh::Often() ? Byte(1 + mh::Next() % 0xFF) : 0;
    // the ability word: 0x32, 0x33, or 0x32 under a high byte not 0 (a byte
    // test would take it for 0x32)
    {
        const unsigned pick = mh::Next() % 4;
        SetWord(mh::Mem(kAbilityId), pick == 0 ? 0x32u : pick == 1 ? 0x33u : pick == 2 ? (0x32u | (1 + mh::Next() % 0xFF) << 8)
                                                                                      : mh::Next() & 0xFFFF);
    }
    switch (k) {
    // the target side: the enemies (bit 0x40) half the time
    case kDreamBreath_TargetHeight:
        if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Mem(mh::at::kTarget)[0] | 0x40);
        break;
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kBoneDart_Task: case kBoneDartChild_Task: case kElemBreath_Task: case kElemBreathChild_Task:
        sc[1] = Byte(mh::Next() % 2);
        break;
    case kDreamBreath_Task: case kPollen_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kDreamBreathMote_Task: case kPollenMote_Task: sc[1] = 0; break;
    case kBoneDartShaft_Run: case kBoneDartShadow_Run: sc[2] = Byte(mh::Next() % 4); if (mh::Half()) sc[0] = 0; break;
    case kElemBreathEmitter_Task: case kElemBreathEnemy_Task: sc[2] = Byte(mh::Next() % 3); break;
    case kElemBreathMote_Run: case kDreamBreathMote_Run: case kPollenMote_Run:
        sc[2] = Byte(mh::Next() % 3);
        if (mh::Half()) sc[0] = 0;
        break;
    // the count-downs and count-ups: at, below and past their ends
    case kBoneDartShaft_Bounce: case kDreamBreathMote_Start: case kPollenMote_Start: case kPollenMote_Fade:
        if (mh::Often()) sc[9] = Near(1, 0, 1);
        if (k == kPollenMote_Start && mh::Half()) sc[0xB] = 0;
        break;
    case kBoneDartShadow_Follow: case kBoneDartShadow_Fade: {
        // the dart's slot: inside the table two times in three, else past it
        // but inside the image (the reads of 155 and above fault both sides)
        for (unsigned t = 0; t < 4; ++t)
            mh::TaskAt(t)[0xB] = mh::Often() ? Byte(mh::Next() % 48) : Byte(48 + mh::Next() % 107);
        // the dart's step at, below and past the one awaited - written only for
        // a slot whose +2 lies inside the compared regions (below 67)
        if (sc[0xB] < 67 && mh::Often()) {
            unsigned char* const dart = mh::Mem(mh::at::kTasks + sc[0xB] * mh::at::kTaskStride);
            dart[2] = Near(k == kBoneDartShadow_Follow ? 2 : 3, 1, 1);
        }
        break;
    }
    case kElemBreathEmitter_Emit:
        sc[9] = mh::Often() ? Byte(MH_PICK(0x62, 0x63, 0x64, 0x65, 0x67, 0x68, 0x7F, 0x80, 0x81, 0xFF, 0)) : Byte(mh::Next());
        break;
    case kElemBreathMote_Start: {
        const unsigned actor = mh::Mem(mh::at::kActor)[0];
        unsigned char* const enemy = mh::EnemyOf(Byte(actor));
        if (mh::Often()) enemy[0x8C] = Byte(MH_PICK(0x10, 0x72, 0x11, 0x71, 0x0F, 0x73));
        break;
    }
    case kElemBreathMote_Flow: sc[9] = Near(0x10, 2, 1); break;
    case kElemBreathMote_Fade: case kDreamBreathMote_Land: sc[0xA] = Near(1, 1, 1); break;
    case kElemBreathMote_Draw: case kDreamBreathMote_Draw: if (mh::Often()) sc[9] = Near(8, 1, 1); break;
    case kDreamBreathMote_Fly: if (mh::Often()) sc[0xA] = Near(0x10, 1, 1); break;
    case kPollen_Sounds: if (mh::Often()) sc[9] = Byte(MH_PICK(7, 8, 9, 0xF, 0x10, 0x11, 0x17, 0x18, 0x19)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[3].at = Key(g_packets);
    mh::Group group = {
        "magic_s09", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    mh::Run(group);
}

}  // namespace magic_s09
