// BOF3X_SHADOW=magic_s16: MAGIC071..074 through the spell round's shared
// harness (magic_harness.h), once at start-up. docs/magic_s16.md section 4.
//
// The clone table (tools/magic_rows.py --unit MAGIC071..074 --clones), the
// callees the standard set lacks (the draws' GPU and GTE helpers, the actor
// test, the tint, and ours called by address), the four .data dispatch
// tables, the regions (the four pools, the overlays' .data, the tint records,
// the packet pointer and a buffer of the fuzz's own, the scratch words, the
// three records past the enemies that 0x4BBCE0 reads), a seed per role and a
// disturbance of the group's cells. Then a check the harness cannot make: the
// answers (al) of the four allocs and of Magic073_CountReacting.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s16.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s16 {
namespace {

namespace mh = magic_harness;
namespace at = magic_harness::at;

// tools/magic_rows.py --unit MAGIC071 / 072 / 073 / 074 --clones, 2026-09-25
// (capstone: every jump internal, no jump table, nothing REFUSED).
// 0x4B9930: 0x81 bytes
constexpr mh::CallSite kCalls4B9930[] = {{0x69, 0x4B9B10}};
constexpr mh::Imm kImms4B9930[] = {{0x15, 0x4B99C0}, {0x1D, 0x4B1E70}, {0x25, 0x4B1ED0}, {0x2D, 0x4EE8A0}, {0x35, 0x4B8F50}, {0x3D, 0x4F7350}};
// 0x4B99C0: 0x14E bytes
constexpr mh::CallSite kCalls4B99C0[] = {{0x5B, 0x4FBD10}, {0xA5, 0x4BA3E0}, {0xDE, 0x5B93D2}, {0x143, 0x587740}};
// 0x4B9B10: 0x12 bytes; +0xB note: jmp through .data 0x65af20, 1 code entries (a data_tables entry)
// 0x4B9B30: 0x8F bytes
constexpr mh::CallSite kCalls4B9B30[] = {{0x3B, 0x4BA1B0}, {0x59, 0x4B9E30}, {0x83, 0x4B9FC0}};
constexpr mh::Imm kImms4B9B30[] = {{0xF, 0x4B9BC0}, {0x17, 0x4B9D30}, {0x22, 0x4B9DA0}};
// 0x4B9BC0: 0x161 bytes
constexpr mh::CallSite kCalls4B9BC0[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
// 0x4B9D30: 0x6C bytes
constexpr mh::CallSite kCalls4B9D30[] = {{0x21, 0x5A7A00}};
// 0x4B9DA0: 0x85 bytes
constexpr mh::CallSite kCalls4B9DA0[] = {{0x21, 0x5A7A00}, {0x7F, 0x4BA430}};
// 0x4B9E30: 0x185 bytes
constexpr mh::CallSite kCalls4B9E30[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
// 0x4B9FC0: 0x1ED bytes
constexpr mh::CallSite kCalls4B9FC0[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
// 0x4BA1B0: 0x22F bytes
constexpr mh::CallSite kCalls4BA1B0[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
// 0x4BA3E0: 0x4A bytes
// 0x4BA430: 0x2F bytes
// 0x4BA460: 0x81 bytes
constexpr mh::CallSite kCalls4BA460[] = {{0x69, 0x4BA640}};
constexpr mh::Imm kImms4BA460[] = {{0x15, 0x4BA4F0}, {0x1D, 0x4B1E70}, {0x25, 0x4B1ED0}, {0x2D, 0x4EE8A0}, {0x35, 0x4B8F50}, {0x3D, 0x4F7350}};
// 0x4BA4F0: 0x14E bytes
constexpr mh::CallSite kCalls4BA4F0[] = {{0x5B, 0x4FBD10}, {0xA5, 0x4BAF10}, {0xDE, 0x5B93D2}, {0x143, 0x587740}};
// 0x4BA640: 0x12 bytes; +0xB note: jmp through .data 0x65b018, 1 code entries (a data_tables entry)
// 0x4BA660: 0x8F bytes
constexpr mh::CallSite kCalls4BA660[] = {{0x3B, 0x4BACE0}, {0x59, 0x4BA960}, {0x83, 0x4BAAF0}};
constexpr mh::Imm kImms4BA660[] = {{0xF, 0x4BA6F0}, {0x17, 0x4BA860}, {0x22, 0x4BA8D0}};
// 0x4BA6F0: 0x161 bytes
constexpr mh::CallSite kCalls4BA6F0[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
// 0x4BA860: 0x6C bytes
constexpr mh::CallSite kCalls4BA860[] = {{0x21, 0x5A7A00}};
// 0x4BA8D0: 0x85 bytes
constexpr mh::CallSite kCalls4BA8D0[] = {{0x21, 0x5A7A00}, {0x7F, 0x4BAF60}};
// 0x4BA960: 0x185 bytes
constexpr mh::CallSite kCalls4BA960[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
// 0x4BAAF0: 0x1ED bytes
constexpr mh::CallSite kCalls4BAAF0[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
// 0x4BACE0: 0x22F bytes
constexpr mh::CallSite kCalls4BACE0[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
// 0x4BAF10: 0x4A bytes
// 0x4BAF60: 0x2F bytes
// 0x4BAF90: 0x61 bytes
constexpr mh::CallSite kCalls4BAF90[] = {{0x49, 0x4BB390}};
constexpr mh::Imm kImms4BAF90[] = {{0x15, 0x4BB000}, {0x1D, 0x4BB170}};
// 0x4BB000: 0x162 bytes
constexpr mh::CallSite kCalls4BB000[] = {{0x58, 0x4456C0}, {0x68, 0x435180}, {0xCB, 0x587900}, {0xE4, 0x4456C0}, {0xF4, 0x435180}, {0x155, 0x587900}};
// 0x4BB170: 0x22 bytes
constexpr mh::CallSite kCalls4BB170[] = {{0xC, 0x4BBCE0}, {0x1C, 0x4351F0}};
// 0x4BB1A0: 0x12 bytes; +0xB note: jmp through .data 0x65b0e0, 2 code entries (a data_tables entry)
// 0x4BB1C0: 0x46 bytes
constexpr mh::Imm kImms4BB1C0[] = {{0xF, 0x4BB210}, {0x17, 0x4BC110}, {0x22, 0x4F4DA0}, {0x2A, 0x4BB370}, {0x32, 0x4BC1A0}, {0x3A, 0x4BC260}};
// 0x4BB210: 0x15F bytes
constexpr mh::CallSite kCalls4BB210[] = {{0x7D, 0x4FBD10}, {0xC7, 0x4BBC60}, {0x100, 0x5B93D2}};
// 0x4BB370: 0x18 bytes
// 0x4BB390: 0x12 bytes; +0xB note: jmp through .data 0x65b0e4, 1 code entries (a data_tables entry)
// 0x4BB3B0: 0x8F bytes
constexpr mh::CallSite kCalls4BB3B0[] = {{0x3B, 0x4BBA30}, {0x59, 0x4BB6B0}, {0x83, 0x4BB840}};
constexpr mh::Imm kImms4BB3B0[] = {{0xF, 0x4BB440}, {0x17, 0x4BB5B0}, {0x22, 0x4BB620}};
// 0x4BB440: 0x161 bytes
constexpr mh::CallSite kCalls4BB440[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
// 0x4BB5B0: 0x6C bytes
constexpr mh::CallSite kCalls4BB5B0[] = {{0x21, 0x5A7A00}};
// 0x4BB620: 0x85 bytes
constexpr mh::CallSite kCalls4BB620[] = {{0x21, 0x5A7A00}, {0x7F, 0x4BBCB0}};
// 0x4BB6B0: 0x185 bytes
constexpr mh::CallSite kCalls4BB6B0[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
// 0x4BB840: 0x1ED bytes
constexpr mh::CallSite kCalls4BB840[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
// 0x4BBA30: 0x22F bytes
constexpr mh::CallSite kCalls4BBA30[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
// 0x4BBC60: 0x4A bytes
// 0x4BBCB0: 0x2F bytes
// 0x4BBCE0: 0x78 bytes
constexpr mh::CallSite kCalls4BBCE0[] = {{0x16, 0x4456C0}, {0x48, 0x4456C0}};
// 0x4BBD60: 0x61 bytes
constexpr mh::CallSite kCalls4BBD60[] = {{0x49, 0x4BC2A0}};
constexpr mh::Imm kImms4BBD60[] = {{0x15, 0x4BBDD0}, {0x1D, 0x4E5200}};
// 0x4BBDD0: 0x162 bytes
constexpr mh::CallSite kCalls4BBDD0[] = {{0x58, 0x4456C0}, {0x68, 0x435180}, {0xCB, 0x587900}, {0xE4, 0x4456C0}, {0xF4, 0x435180}, {0x155, 0x587900}};
// 0x4BBF40: 0x12 bytes; +0xB note: jmp through .data 0x65b1ac, 10 code entries (a data_tables entry)
// 0x4BBF60: 0x46 bytes
constexpr mh::Imm kImms4BBF60[] = {{0xF, 0x4BBFB0}, {0x17, 0x4BC110}, {0x22, 0x4F4DA0}, {0x2A, 0x4BB370}, {0x32, 0x4BC1A0}, {0x3A, 0x4BC260}};
// 0x4BBFB0: 0x15F bytes
constexpr mh::CallSite kCalls4BBFB0[] = {{0x7D, 0x4FBD10}, {0xC7, 0x4BCB70}, {0x100, 0x5B93D2}};
// 0x4BC110: 0x82 bytes
constexpr mh::CallSite kCalls4BC110[] = {{0x50, 0x454DC0}, {0x5E, 0x454CC0}};
// 0x4BC1A0: 0xBA bytes
constexpr mh::CallSite kCalls4BC1A0[] = {{0x9A, 0x454DC0}, {0xA9, 0x4FBDB0}};
// 0x4BC260: 0x31 bytes
constexpr mh::CallSite kCalls4BC260[] = {{0x23, 0x4530D0}, {0x2B, 0x4351F0}};
// 0x4BC2A0: 0x12 bytes; +0xB note: jmp through .data 0x65b1b0, 9 code entries (a data_tables entry)
// 0x4BC2C0: 0x8F bytes
constexpr mh::CallSite kCalls4BC2C0[] = {{0x3B, 0x4BC940}, {0x59, 0x4BC5C0}, {0x83, 0x4BC750}};
constexpr mh::Imm kImms4BC2C0[] = {{0xF, 0x4BC350}, {0x17, 0x4BC4C0}, {0x22, 0x4BC530}};
// 0x4BC350: 0x161 bytes
constexpr mh::CallSite kCalls4BC350[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
// 0x4BC4C0: 0x6C bytes
constexpr mh::CallSite kCalls4BC4C0[] = {{0x21, 0x5A7A00}};
// 0x4BC530: 0x85 bytes
constexpr mh::CallSite kCalls4BC530[] = {{0x21, 0x5A7A00}, {0x7F, 0x4BCBC0}};
// 0x4BC5C0: 0x185 bytes
constexpr mh::CallSite kCalls4BC5C0[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
// 0x4BC750: 0x1ED bytes
constexpr mh::CallSite kCalls4BC750[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
// 0x4BC940: 0x22F bytes
constexpr mh::CallSite kCalls4BC940[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
// 0x4BCB70: 0x4A bytes
// 0x4BCBC0: 0x2F bytes
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Magic071_Task", 0x4B9930, 0x81, kCalls4B9930, MH_N(kCalls4B9930), kImms4B9930, MH_N(kImms4B9930), nullptr, 0, reinterpret_cast<const void*>(&::Magic071_Task)},
    {"Magic071_Spawn", 0x4B99C0, 0x14E, kCalls4B99C0, MH_N(kCalls4B99C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_Spawn)},
    {"Magic071_SparkleDispatch", 0x4B9B10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleDispatch)},
    {"Magic071_SparkleUpdate", 0x4B9B30, 0x8F, kCalls4B9B30, MH_N(kCalls4B9B30), kImms4B9B30, MH_N(kImms4B9B30), nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleUpdate)},
    {"Magic071_SparkleLaunch", 0x4B9BC0, 0x161, kCalls4B9BC0, MH_N(kCalls4B9BC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleLaunch)},
    {"Magic071_SparkleRise", 0x4B9D30, 0x6C, kCalls4B9D30, MH_N(kCalls4B9D30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleRise)},
    {"Magic071_SparkleFade", 0x4B9DA0, 0x85, kCalls4B9DA0, MH_N(kCalls4B9DA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleFade)},
    {"Magic071_SparkleRaysG2", 0x4B9E30, 0x185, kCalls4B9E30, MH_N(kCalls4B9E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleRaysG2)},
    {"Magic071_SparkleRaysG3", 0x4B9FC0, 0x1ED, kCalls4B9FC0, MH_N(kCalls4B9FC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleRaysG3)},
    {"Magic071_SparkleDisc", 0x4BA1B0, 0x22F, kCalls4BA1B0, MH_N(kCalls4BA1B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleDisc)},
    {"Magic071_SparkleAlloc", 0x4BA3E0, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleAlloc)},
    {"Magic071_SparkleFree", 0x4BA430, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic071_SparkleFree)},
    {"Magic072_Task", 0x4BA460, 0x81, kCalls4BA460, MH_N(kCalls4BA460), kImms4BA460, MH_N(kImms4BA460), nullptr, 0, reinterpret_cast<const void*>(&::Magic072_Task)},
    {"Magic072_Spawn", 0x4BA4F0, 0x14E, kCalls4BA4F0, MH_N(kCalls4BA4F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_Spawn)},
    {"Magic072_SparkleDispatch", 0x4BA640, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleDispatch)},
    {"Magic072_SparkleUpdate", 0x4BA660, 0x8F, kCalls4BA660, MH_N(kCalls4BA660), kImms4BA660, MH_N(kImms4BA660), nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleUpdate)},
    {"Magic072_SparkleLaunch", 0x4BA6F0, 0x161, kCalls4BA6F0, MH_N(kCalls4BA6F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleLaunch)},
    {"Magic072_SparkleRise", 0x4BA860, 0x6C, kCalls4BA860, MH_N(kCalls4BA860), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleRise)},
    {"Magic072_SparkleFade", 0x4BA8D0, 0x85, kCalls4BA8D0, MH_N(kCalls4BA8D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleFade)},
    {"Magic072_SparkleRaysG2", 0x4BA960, 0x185, kCalls4BA960, MH_N(kCalls4BA960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleRaysG2)},
    {"Magic072_SparkleRaysG3", 0x4BAAF0, 0x1ED, kCalls4BAAF0, MH_N(kCalls4BAAF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleRaysG3)},
    {"Magic072_SparkleDisc", 0x4BACE0, 0x22F, kCalls4BACE0, MH_N(kCalls4BACE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleDisc)},
    {"Magic072_SparkleAlloc", 0x4BAF10, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleAlloc)},
    {"Magic072_SparkleFree", 0x4BAF60, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic072_SparkleFree)},
    {"Magic073_Task", 0x4BAF90, 0x61, kCalls4BAF90, MH_N(kCalls4BAF90), kImms4BAF90, MH_N(kImms4BAF90), nullptr, 0, reinterpret_cast<const void*>(&::Magic073_Task)},
    {"Magic073_Spawn", 0x4BB000, 0x162, kCalls4BB000, MH_N(kCalls4BB000), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_Spawn)},
    {"Magic073_Wait", 0x4BB170, 0x22, kCalls4BB170, MH_N(kCalls4BB170), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_Wait)},
    {"Magic073_ActorDispatch", 0x4BB1A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_ActorDispatch)},
    {"Magic073_ActorTask", 0x4BB1C0, 0x46, nullptr, 0, kImms4BB1C0, MH_N(kImms4BB1C0), nullptr, 0, reinterpret_cast<const void*>(&::Magic073_ActorTask)},
    {"Magic073_ActorStart", 0x4BB210, 0x15F, kCalls4BB210, MH_N(kCalls4BB210), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_ActorStart)},
    {"ActorFx_WaitStep4", 0x4BB370, 0x18, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ActorFx_WaitStep4)},
    {"Magic073_SparkleDispatch", 0x4BB390, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleDispatch)},
    {"Magic073_SparkleUpdate", 0x4BB3B0, 0x8F, kCalls4BB3B0, MH_N(kCalls4BB3B0), kImms4BB3B0, MH_N(kImms4BB3B0), nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleUpdate)},
    {"Magic073_SparkleLaunch", 0x4BB440, 0x161, kCalls4BB440, MH_N(kCalls4BB440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleLaunch)},
    {"Magic073_SparkleRise", 0x4BB5B0, 0x6C, kCalls4BB5B0, MH_N(kCalls4BB5B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleRise)},
    {"Magic073_SparkleFade", 0x4BB620, 0x85, kCalls4BB620, MH_N(kCalls4BB620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleFade)},
    {"Magic073_SparkleRaysG2", 0x4BB6B0, 0x185, kCalls4BB6B0, MH_N(kCalls4BB6B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleRaysG2)},
    {"Magic073_SparkleRaysG3", 0x4BB840, 0x1ED, kCalls4BB840, MH_N(kCalls4BB840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleRaysG3)},
    {"Magic073_SparkleDisc", 0x4BBA30, 0x22F, kCalls4BBA30, MH_N(kCalls4BBA30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleDisc)},
    {"Magic073_SparkleAlloc", 0x4BBC60, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleAlloc)},
    {"Magic073_SparkleFree", 0x4BBCB0, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_SparkleFree)},
    {"Magic073_CountReacting", 0x4BBCE0, 0x78, kCalls4BBCE0, MH_N(kCalls4BBCE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic073_CountReacting)},
    {"Magic074_Task", 0x4BBD60, 0x61, kCalls4BBD60, MH_N(kCalls4BBD60), kImms4BBD60, MH_N(kImms4BBD60), nullptr, 0, reinterpret_cast<const void*>(&::Magic074_Task)},
    {"Magic074_Spawn", 0x4BBDD0, 0x162, kCalls4BBDD0, MH_N(kCalls4BBDD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_Spawn)},
    {"Magic074_ActorDispatch", 0x4BBF40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_ActorDispatch)},
    {"Magic074_ActorTask", 0x4BBF60, 0x46, nullptr, 0, kImms4BBF60, MH_N(kImms4BBF60), nullptr, 0, reinterpret_cast<const void*>(&::Magic074_ActorTask)},
    {"Magic074_ActorStart", 0x4BBFB0, 0x15F, kCalls4BBFB0, MH_N(kCalls4BBFB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_ActorStart)},
    {"ActorFx_Tint", 0x4BC110, 0x82, kCalls4BC110, MH_N(kCalls4BC110), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ActorFx_Tint)},
    {"ActorFx_Untint", 0x4BC1A0, 0xBA, kCalls4BC1A0, MH_N(kCalls4BC1A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ActorFx_Untint)},
    {"ActorFx_End", 0x4BC260, 0x31, kCalls4BC260, MH_N(kCalls4BC260), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ActorFx_End)},
    {"Magic074_SparkleDispatch", 0x4BC2A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleDispatch)},
    {"Magic074_SparkleUpdate", 0x4BC2C0, 0x8F, kCalls4BC2C0, MH_N(kCalls4BC2C0), kImms4BC2C0, MH_N(kImms4BC2C0), nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleUpdate)},
    {"Magic074_SparkleLaunch", 0x4BC350, 0x161, kCalls4BC350, MH_N(kCalls4BC350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleLaunch)},
    {"Magic074_SparkleRise", 0x4BC4C0, 0x6C, kCalls4BC4C0, MH_N(kCalls4BC4C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleRise)},
    {"Magic074_SparkleFade", 0x4BC530, 0x85, kCalls4BC530, MH_N(kCalls4BC530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleFade)},
    {"Magic074_SparkleRaysG2", 0x4BC5C0, 0x185, kCalls4BC5C0, MH_N(kCalls4BC5C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleRaysG2)},
    {"Magic074_SparkleRaysG3", 0x4BC750, 0x1ED, kCalls4BC750, MH_N(kCalls4BC750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleRaysG3)},
    {"Magic074_SparkleDisc", 0x4BC940, 0x22F, kCalls4BC940, MH_N(kCalls4BC940), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleDisc)},
    {"Magic074_SparkleAlloc", 0x4BCB70, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleAlloc)},
    {"Magic074_SparkleFree", 0x4BCBC0, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic074_SparkleFree)},
};
#undef MH_N
enum : unsigned {
    kMagic071_Task,
    kMagic071_Spawn,
    kMagic071_SparkleDispatch,
    kMagic071_SparkleUpdate,
    kMagic071_SparkleLaunch,
    kMagic071_SparkleRise,
    kMagic071_SparkleFade,
    kMagic071_SparkleRaysG2,
    kMagic071_SparkleRaysG3,
    kMagic071_SparkleDisc,
    kMagic071_SparkleAlloc,
    kMagic071_SparkleFree,
    kMagic072_Task,
    kMagic072_Spawn,
    kMagic072_SparkleDispatch,
    kMagic072_SparkleUpdate,
    kMagic072_SparkleLaunch,
    kMagic072_SparkleRise,
    kMagic072_SparkleFade,
    kMagic072_SparkleRaysG2,
    kMagic072_SparkleRaysG3,
    kMagic072_SparkleDisc,
    kMagic072_SparkleAlloc,
    kMagic072_SparkleFree,
    kMagic073_Task,
    kMagic073_Spawn,
    kMagic073_Wait,
    kMagic073_ActorDispatch,
    kMagic073_ActorTask,
    kMagic073_ActorStart,
    kActorFx_WaitStep4,
    kMagic073_SparkleDispatch,
    kMagic073_SparkleUpdate,
    kMagic073_SparkleLaunch,
    kMagic073_SparkleRise,
    kMagic073_SparkleFade,
    kMagic073_SparkleRaysG2,
    kMagic073_SparkleRaysG3,
    kMagic073_SparkleDisc,
    kMagic073_SparkleAlloc,
    kMagic073_SparkleFree,
    kMagic073_CountReacting,
    kMagic074_Task,
    kMagic074_Spawn,
    kMagic074_ActorDispatch,
    kMagic074_ActorTask,
    kMagic074_ActorStart,
    kActorFx_Tint,
    kActorFx_Untint,
    kActorFx_End,
    kMagic074_SparkleDispatch,
    kMagic074_SparkleUpdate,
    kMagic074_SparkleLaunch,
    kMagic074_SparkleRise,
    kMagic074_SparkleFade,
    kMagic074_SparkleRaysG2,
    kMagic074_SparkleRaysG3,
    kMagic074_SparkleDisc,
    kMagic074_SparkleAlloc,
    kMagic074_SparkleFree,
};

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// A callee ours reaches by its name (Capcom's address it displaced, our function).
#define S16_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define S16_POOL_CALLEES(N)                                                                          \
    {S16_OURS(Magic##N##_SparkleDispatch), 0, {}, mh::Answer::kGarbage, 0, 0},                        \
    {S16_OURS(Magic##N##_SparkleDisc), 0, {}, mh::Answer::kGarbage, 0, 0},                            \
    {S16_OURS(Magic##N##_SparkleRaysG2), 2, {kU16, kU16}, mh::Answer::kGarbage, 0, 0},                \
    {S16_OURS(Magic##N##_SparkleRaysG3), 2, {kU16, kU16}, mh::Answer::kGarbage, 0, 0},                \
    {S16_OURS(Magic##N##_SparkleAlloc), 0, {}, mh::Answer::kByte, 0, 0xFF},                           \
    {S16_OURS(Magic##N##_SparkleFree), 0, {}, mh::Answer::kGarbage, 0, 0}
const mh::Callee kCallees[] = {
    // the draws' (round seven's and eight's)
    {S16_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},   // the fifth (tw, 0) unlogged
    {S16_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S16_OURS(Gpu_SetLineG2), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S16_OURS(Gpu_SetLineG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S16_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S16_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S16_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S16_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    // the actors'
    {S16_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S16_OURS(Sprite_SetTint), 4, {kAll, kU8, kU8, kU8}, mh::Answer::kGarbage, 0, 0},   // the fifth (a, 1) unlogged
    // ours, called by the clones' E8 / E9
    S16_POOL_CALLEES(071),
    S16_POOL_CALLEES(072),
    S16_POOL_CALLEES(073),
    S16_POOL_CALLEES(074),
    {S16_OURS(Magic073_CountReacting), 0, {}, mh::Answer::kFlag, 0, 0},
};
#undef S16_POOL_CALLEES

// The dispatches through .data (read in place): swapped for recorders. The
// child tables' second entries are the sparkle tables' first.
const mh::DataTable kTables[] = {{0x65AF20, 1}, {0x65B018, 1}, {0x65B0E0, 2}, {0x65B1AC, 2}};

// Where the four pools are, and what each one's records hold.
struct PoolAt { std::uint32_t pool; unsigned count; std::uint32_t current; };
constexpr PoolAt kPools[4] = {{0x685D98, 0x80, 0x687398}, {0x6873A0, 0x80, 0x6889A0}, {0x6889A8, 0xA0, 0x68A528}, {0x68A530, 0xA0, 0x68C0B0}};
constexpr std::uint32_t kStride = 0x2C;
// The pools and cells, and as far past the last as a spawn's index of 254
// writes (0x68A530 + 0x2C00).
constexpr std::uint32_t kPoolsLo = 0x685D98, kPoolsHi = 0x68A530 + 0x100 * kStride;
// The overlays' .data (their tables), less the four dispatch cells above.
constexpr mh::Region kData[] = {{0x65AE2C, 0xF4}, {0x65AF24, 0xF4}, {0x65B01C, 0xC4}, {0x65B0E8, 0xC4}};

alignas(16) unsigned char g_prims[0x100];
constexpr std::uint32_t kPacketNext = 0x7E0670;
constexpr std::uint32_t kScratch = 0x903850;
constexpr std::uint32_t kEnemyState = 0x93BCD9;   // Magic073_CountReacting's (0x93B961 + 0x128 * 3)
constexpr std::uint32_t kPastEnemies = at::kEnemies + 8 * at::kEnemyStride;

mh::Region g_regions[10];

// The .data as the exe holds it, read at start-up (not written down here):
// random counts make every spawn run to 255.
unsigned char g_exe_data[4][0xF4];

unsigned char* Record(unsigned pool, unsigned i) { return mh::Mem(kPools[pool].pool + i * kStride); }
unsigned char* Cur(unsigned pool) { return mh::Pointer(kPools[pool].current); }
unsigned char* ValidOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }

// What a function is, to the seed: by its name.
enum Role : unsigned {
    kTask6, kTask2, kSpawn, kSpawnActors, kWait, kActorDispatch, kActorTask, kActorStart, kWaitStep4, kCount,
    kTint, kUntint, kEnd, kDispatch, kUpdate, kLaunch, kRise, kFade, kDraw, kAlloc, kFree,
};
struct Kind { Role role; unsigned pool; };
Kind g_kind[sizeof kClones / sizeof kClones[0]];

bool Has(const char* name, const char* part) { return std::strstr(name, part) != nullptr; }
Kind KindOf(const char* n) {
    const unsigned pool = Has(n, "071") ? 0 : Has(n, "072") ? 1 : Has(n, "073") ? 2 : 3;
    if (Has(n, "_Task")) return {pool < 2 ? kTask6 : kTask2, pool};
    if (Has(n, "_Spawn")) return {pool < 2 ? kSpawn : kSpawnActors, pool};
    if (Has(n, "_Wait") && !Has(n, "Step")) return {kWait, pool};
    if (Has(n, "ActorDispatch")) return {kActorDispatch, pool};
    if (Has(n, "ActorTask")) return {kActorTask, pool};
    if (Has(n, "ActorStart")) return {kActorStart, pool};
    if (Has(n, "WaitStep4")) return {kWaitStep4, pool};
    if (Has(n, "CountReacting")) return {kCount, pool};
    if (Has(n, "ActorFx_Tint")) return {kTint, pool};
    if (Has(n, "ActorFx_Untint")) return {kUntint, pool};
    if (Has(n, "ActorFx_End")) return {kEnd, pool};
    if (Has(n, "SparkleDispatch")) return {kDispatch, pool};
    if (Has(n, "SparkleUpdate")) return {kUpdate, pool};
    if (Has(n, "SparkleLaunch")) return {kLaunch, pool};
    if (Has(n, "SparkleRise")) return {kRise, pool};
    if (Has(n, "SparkleFade")) return {kFade, pool};
    if (Has(n, "SparkleRays") || Has(n, "SparkleDisc")) return {kDraw, pool};
    if (Has(n, "SparkleAlloc")) return {kAlloc, pool};
    if (Has(n, "SparkleFree")) return {kFree, pool};
    bof3::Fatal("magic_s16: no role for %s", n);
}

// A pool's records with bit 0 set in the first `m`, the rest random.
void Occupy(unsigned pool, unsigned m) {
    for (unsigned i = 0; i < m && i < kPools[pool].count; ++i) Record(pool, i)[0] |= 1;
}

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    // Every round: the packet pointer into the fuzz's buffer; each pool's
    // records owned by a real slot or record (the walk makes them the owner a
    // recorder may write through); each current cell a record of its pool,
    // type 0 (the dispatch tables hold one entry); the exe's .data two in three.
    Gfx_PacketNext = g_prims + (mh::Half() ? 0 : 0x40);
    for (unsigned p = 0; p < 4; ++p) {
        for (unsigned i = 0; i < kPools[p].count; ++i) mh::SetPointer(kPools[p].pool + i * kStride + 0x28, ValidOwner(mh::Next()));
        mh::SetPointer(kPools[p].current, Record(p, mh::Next() % kPools[p].count));
        Cur(p)[1] = 0;
    }
    if (mh::Often())
        for (unsigned i = 0; i < 4; ++i) std::memcpy(mh::Mem(kData[i].at), g_exe_data[i], kData[i].size);
    const Kind kind = g_kind[k];
    unsigned char* const c = Cur(kind.pool);
    switch (kind.role) {
    case kTask6: sc[1] = static_cast<unsigned char>(mh::Next() % 6); break;
    case kTask2: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kSpawn: break;
    case kSpawnActors:
        if (mh::Half()) mh::Mem(at::kTarget)[0] |= 0x40;
        break;
    case kWait: if (mh::Half()) sc[0xB] = 0; break;
    case kActorDispatch: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kActorTask: sc[2] = static_cast<unsigned char>(mh::Next() % 6); break;
    case kActorStart:
        if (mh::Half()) sc[9] = 1;
        sc[4] = static_cast<unsigned char>(mh::Next() % 11);
        if (mh::Often()) mh::Pointer(at::kOwner)[4] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kWaitStep4: sc[0xB] = static_cast<unsigned char>(MH_PICK(3, 4, 5, 6, 0x85, 0)); break;
    case kCount:
        for (unsigned i = 0; i < 8; ++i)
            if (mh::Half()) mh::Mem(kEnemyState + i * at::kEnemyStride)[0] = 6;
        for (unsigned i = 0; i < 3; ++i)
            if (mh::Half()) mh::Mem(at::kParty + 1 + i * at::kPartyStride)[0] = 6;
        break;
    case kTint:
    case kUntint:
        if (mh::Half()) sc[9] = 1;
        sc[4] = static_cast<unsigned char>(mh::Next() % 11);
        break;
    case kEnd: if (mh::Half()) sc[0xB] = 0; break;
    case kDispatch: break;
    case kUpdate:
        c[2] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kLaunch:
        if (mh::Half()) c[5] = 1;
        break;
    case kRise:
        if (mh::Half()) c[7] = static_cast<unsigned char>(c[6] + 1);
        break;
    case kFade:
        if (mh::Half()) c[5] = 1;
        break;
    case kDraw:
        if (mh::Often()) {
            c[3] &= 3;
            c[4] = static_cast<unsigned char>(c[4] % 5);
        }
        break;
    case kAlloc:
        if (mh::Half()) Occupy(kind.pool, mh::Half() ? kPools[kind.pool].count : mh::Next() % kPools[kind.pool].count);
        break;
    case kFree: break;
    }
}

// After a call, two in three: move a pool's current cell, the packet pointer,
// a scratch word, or a byte of a current sparkle (not its owner +0x28, which
// the walk hands on).
void Disturb(std::uint32_t h) {
    const unsigned p = (h >> 12) % 4;
    switch ((h >> 8) % 4) {
    case 0: mh::SetPointer(kPools[p].current, Record(p, (h >> 16) % kPools[p].count)); break;
    case 1: Gfx_PacketNext = g_prims + ((h >> 14) & 1 ? 0 : 0x40); break;
    case 2: move_script::SetWord(mh::Mem(kScratch + 2 * ((h >> 16) % 8)), h >> 20); break;
    default: {
        // through the cell only while it points into its pool: a spawn's
        // index past the pool (an alloc answering 128..254) writes over it
        const std::uint32_t c = static_cast<std::uint32_t>(move_script::Long(mh::Mem(kPools[p].current)));
        if (c >= kPools[p].pool && c < kPools[p].pool + kPools[p].count * kStride)
            Cur(p)[(h >> 16) % 0x28] = static_cast<unsigned char>(h >> 24);
        break;
    }
    }
}

// --- the answers --------------------------------------------------------------
//
// The harness compares memory and the recorders' log, not eax. The four allocs
// and the count answer in al: each is cloned plainly (the count's two calls
// kept on Battle_ActorIsOut, ours, which reads the records and nothing else)
// and run against ours from the same random pool, party and enemy bytes.
void CheckAnswers() {
    struct Answering { const char* name; std::uint32_t base, size; unsigned char (__cdecl* ours)(); unsigned pool; };
    const Answering kAnswers[] = {
        {"Magic071_SparkleAlloc", 0x4BA3E0, 0x4A, &::Magic071_SparkleAlloc, 0},
        {"Magic072_SparkleAlloc", 0x4BAF10, 0x4A, &::Magic072_SparkleAlloc, 1},
        {"Magic073_SparkleAlloc", 0x4BBC60, 0x4A, &::Magic073_SparkleAlloc, 2},
        {"Magic074_SparkleAlloc", 0x4BCB70, 0x4A, &::Magic074_SparkleAlloc, 3},
        {"Magic073_CountReacting", 0x4BBCE0, 0x78, &::Magic073_CountReacting, 4},
    };
    const bof3::CloneCall count_calls[] = {{0x16, reinterpret_cast<const void*>(&::Battle_ActorIsOut), 0x4456C0},
                                           {0x48, reinterpret_cast<const void*>(&::Battle_ActorIsOut), 0x4456C0}};
    // the bytes they read and write, kept and put back
    const mh::Region kept_at[] = {{kPoolsLo, kPoolsHi - kPoolsLo}, {at::kParty, 3 * at::kPartyStride}, {at::kEnemies, 11 * at::kEnemyStride}};
    static unsigned char kept[kPoolsHi - kPoolsLo + 3 * 0x14C + 11 * 0x128];
    static unsigned char after_theirs[kPoolsHi - kPoolsLo];
    static unsigned char g_state_in[kPoolsHi - kPoolsLo];
    unsigned n = 0;
    for (const mh::Region& r : kept_at) { std::memcpy(kept + n, mh::Mem(r.at), r.size); n += r.size; }
    unsigned bad = 0, rounds = 0, full = 0, reacting = 0;
    for (const Answering& a : kAnswers) {
        const bool count = a.pool == 4;
        void* const copy = bof3::CloneOriginal(a.name, a.base, a.size, count ? count_calls : nullptr, count ? 2 : 0);
        const auto theirs = reinterpret_cast<std::uint32_t (__cdecl*)()>(copy);
        for (unsigned round = 0; round < 2000; ++round, ++rounds) {
            for (const mh::Region& r : kept_at)
                for (unsigned i = 0; i < r.size; ++i) mh::Mem(r.at)[i] = static_cast<unsigned char>(mh::Next());
            if (!count && mh::Half()) Occupy(a.pool, mh::Half() ? kPools[a.pool].count : mh::Next() % kPools[a.pool].count);
            if (count)
                for (unsigned i = 0; i < 8; ++i)
                    if (mh::Half()) mh::Mem(kEnemyState + i * at::kEnemyStride)[0] = 6;
            std::memcpy(g_state_in, mh::Mem(kPoolsLo), sizeof g_state_in);
            const unsigned char t = static_cast<unsigned char>(theirs());
            std::memcpy(after_theirs, mh::Mem(kPoolsLo), sizeof after_theirs);
            std::memcpy(mh::Mem(kPoolsLo), g_state_in, sizeof g_state_in);
            const unsigned char o = a.ours();
            if (t == 0xFF && !count) ++full;
            if (count && t) ++reacting;
            if (t != o || std::memcmp(after_theirs, mh::Mem(kPoolsLo), sizeof after_theirs) != 0) {
                if (++bad <= 12) bof3::Log("shadow      magic_s16 answers MISMATCH: %s round %u, theirs %u ours %u", a.name, round, t, o);
            }
        }
    }
    n = 0;
    for (const mh::Region& r : kept_at) { std::memcpy(mh::Mem(r.at), kept + n, r.size); n += r.size; }
    bof3::Log("shadow      magic_s16 answers: %u rounds over 5 functions (the allocs' al and pool, the count's al), %u full pools, "
              "%u counts not 0, %u MISMATCHES", rounds, full, reacting, bad);
    if (bad) bof3::Fatal("magic_s16's answers differ from the originals' in %u rounds", bad);
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < 4; ++i) std::memcpy(g_exe_data[i], mh::Mem(kData[i].at), kData[i].size);
    for (unsigned k = 0; k < sizeof kClones / sizeof kClones[0]; ++k) g_kind[k] = KindOf(kClones[k].name);
    unsigned r = 0;
    g_regions[r++] = {kPoolsLo, kPoolsHi - kPoolsLo};
    for (const mh::Region& d : kData) g_regions[r++] = d;
    g_regions[r++] = {0x7E0700, 0xC00};                       // MoveScript_TintRecords, by an unbounded +0xA
    g_regions[r++] = {kPacketNext, 4};                        // Gfx_PacketNext
    g_regions[r++] = {Key(g_prims), sizeof g_prims};
    g_regions[r++] = {kScratch, 0x10};
    g_regions[r++] = {kPastEnemies, 3 * at::kEnemyStride};    // records 8..10: 0x4BBCE0's reach
    const mh::Group group = {
        "magic_s16", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], g_regions, r, &Seed, &Disturb, 2000,
    };
    mh::Run(group);
    CheckAnswers();
}

}  // namespace magic_s16
