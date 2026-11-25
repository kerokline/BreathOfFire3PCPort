// The came-from place in a save file's tail (save_tail.h; ledger draft C,
// docs/state-by-name-tier0.md 3).
#include "game/save_tail.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace save_tail {

namespace {

constexpr unsigned char kMagic[8] = {'B', 'O', 'F', '3', 'X', 'T', 'L', 0};
constexpr std::size_t kRecordsRoom = kTailBytes - kHeadBytes;   // 0x1F0
constexpr std::uint16_t kCameFromBytes = 12;

std::uint32_t Fnv(const unsigned char* p, std::size_t n) {
    std::uint32_t h = 0x811C9DC5u;
    for (std::size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}

void Put16(unsigned char* p, std::uint16_t v) { std::memcpy(p, &v, 2); }
void Put32(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, 4); }
std::uint16_t Get16(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}
std::uint32_t Get32(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

}  // namespace

void Build(unsigned char* tail, const CameFrom& from) {
    std::memset(tail, 0, kTailBytes);
    unsigned char* const r = tail + kHeadBytes;
    Put16(r, kKindCameFrom);
    Put16(r + 2, kCameFromBytes);
    Put16(r + 4, from.area);
    Put16(r + 6, 0);
    Put32(r + 8, static_cast<std::uint32_t>(from.x));
    Put32(r + 12, static_cast<std::uint32_t>(from.z));
    const std::uint16_t records = 4 + kCameFromBytes;
    std::memcpy(tail, kMagic, sizeof kMagic);
    Put16(tail + 8, kVersion);
    Put16(tail + 0xA, records);
    Put32(tail + 0xC, Fnv(r, records));
}

Read Parse(const unsigned char* tail, CameFrom& from, bool& has_from, const char*& why) {
    has_from = false;
    why = "";
    if (std::memcmp(tail, kMagic, sizeof kMagic) != 0) {
        why = "no tag";
        return Read::NoTag;
    }
    if (Get16(tail + 8) != kVersion) {
        why = "a version this build does not read";
        return Read::Damaged;
    }
    const std::uint16_t records = Get16(tail + 0xA);
    if (records > kRecordsRoom) {
        why = "the records run past the tail";
        return Read::Damaged;
    }
    const unsigned char* const r = tail + kHeadBytes;
    if (Fnv(r, records) != Get32(tail + 0xC)) {
        why = "the records do not hash to their FNV";
        return Read::Damaged;
    }
    for (std::size_t at = 0; at < records;) {
        if (records - at < 4) {
            why = "a record's head past the records";
            return Read::Damaged;
        }
        const std::uint16_t kind = Get16(r + at), bytes = Get16(r + at + 2);
        if (bytes > records - at - 4) {
            why = "a record past the records";
            return Read::Damaged;
        }
        if (kind == kKindCameFrom) {
            if (bytes < kCameFromBytes) {
                why = "a came-from record too short";
                return Read::Damaged;
            }
            from.area = Get16(r + at + 4);
            from.x = static_cast<std::int32_t>(Get32(r + at + 8));
            from.z = static_cast<std::int32_t>(Get32(r + at + 12));
            has_from = true;
        }
        at += 4u + bytes;
    }
    return Read::Tagged;
}

Due Decide(unsigned mode, std::uint16_t area_now, std::uint16_t area_saved, std::uint16_t last_area) {
    if (mode == 0 || mode == 1) return Due::Wait;
    if (area_now == area_saved && last_area == kNoArea) return Due::PutBack;
    return Due::Drop;
}

}  // namespace save_tail

namespace {

using namespace save_tail;

// The switch (PatchBytes "SaveCameFrom"), 1 once armed after the self-tests.
std::uint8_t g_on = 0;

// A put-back armed by a load (ours, outside the snapshot span: it lives for
// the few frames between the title's read and the loaded game's entry).
bool g_armed = false;
CameFrom g_from;
std::uint16_t g_saved_area = 0;

unsigned char* At(std::uint32_t va) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(va))); }

constexpr std::uint32_t kFromX = 0x21C, kFromZ = 0x220;   // MoveScript_TintRecords + : 0x7E091C, 0x7E0920

CameFrom Live() {
    CameFrom f;
    f.area = AreaW2c_LastArea;
    f.x = static_cast<std::int32_t>(Get32(At(MoveScript_TintRecords_addr) + kFromX));
    f.z = static_cast<std::int32_t>(Get32(At(MoveScript_TintRecords_addr) + kFromZ));
    return f;
}

[[noreturn]] void Fail(const char* what) { bof3::Fatal("save tail self-test: %s", what); }

void SelfTest() {
    unsigned char tail[kTailBytes];
    CameFrom from, got;
    from.area = 0x2A;
    from.x = 0x00123456;
    from.z = -0x00654321;
    bool has = false;
    const char* why = "";
    // Every save measured before this: 0x200 zero bytes. No tag.
    std::memset(tail, 0, sizeof tail);
    if (Parse(tail, got, has, why) != Read::NoTag || has) Fail("a zero tail is not 'no tag'");
    // Written and read back.
    Build(tail, from);
    if (Parse(tail, got, has, why) != Read::Tagged || !has) Fail("a built tail does not read as tagged with its place");
    if (got.area != from.area || got.x != from.x || got.z != from.z) Fail("the came-from place does not read back");
    for (std::size_t i = kHeadBytes + 16; i < kTailBytes; ++i)
        if (tail[i] != 0) Fail("the tail past its records is not zero");
    // Faults: one byte of the place, the version, the length, the FNV.
    unsigned char bad[kTailBytes];
    std::memcpy(bad, tail, sizeof bad);
    bad[kHeadBytes + 5] ^= 1;
    if (Parse(bad, got, has, why) != Read::Damaged) Fail("a changed place byte is not caught by the FNV");
    std::memcpy(bad, tail, sizeof bad);
    bad[8] = 2;
    if (Parse(bad, got, has, why) != Read::Damaged) Fail("another version is read");
    std::memcpy(bad, tail, sizeof bad);
    bad[0xA] = 0xF1;
    bad[0xB] = 0x01;
    if (Parse(bad, got, has, why) != Read::Damaged) Fail("a length past the tail is read");
    std::memcpy(bad, tail, sizeof bad);
    bad[0] = 'b';
    if (Parse(bad, got, has, why) != Read::NoTag) Fail("another magic is not 'no tag'");
    // A record of a kind this build does not know, before the place: skipped,
    // the place still read (a later use of the tail coexists).
    {
        std::memset(bad, 0, sizeof bad);
        unsigned char* r = bad + kHeadBytes;
        Put16(r, 7);
        Put16(r + 2, 3);
        r[4] = r[5] = r[6] = 0xEE;
        std::memcpy(r + 7, tail + kHeadBytes, 16);
        const std::uint16_t n = 7 + 16;
        std::memcpy(bad, tail, 8);
        Put16(bad + 8, kVersion);
        Put16(bad + 0xA, n);
        Put32(bad + 0xC, Fnv(r, n));
        if (Parse(bad, got, has, why) != Read::Tagged || !has || got.area != from.area || got.z != from.z)
            Fail("an unknown record kind is not skipped");
        // Only the unknown record: tagged, no place.
        Put16(bad + 0xA, 7);
        Put32(bad + 0xC, Fnv(r, 7));
        if (Parse(bad, got, has, why) != Read::Tagged || has) Fail("a tail with no place reads a place");
        // A record that runs past the records.
        Put16(r + 2, 9);
        Put32(bad + 0xC, Fnv(r, 7));
        if (Parse(bad, got, has, why) != Read::Damaged) Fail("a record past the records is read");
    }
    // The put-back's condition.
    if (Decide(0, 29, 29, kNoArea) != Due::Wait || Decide(1, 29, 29, kNoArea) != Due::Wait) Fail("modes 0 and 1 do not wait");
    if (Decide(2, 29, 29, kNoArea) != Due::PutBack) Fail("the field in the saved area does not put back");
    if (Decide(8, 29, 29, kNoArea) != Due::PutBack) Fail("mode 8 in the saved area does not put back");
    if (Decide(2, 30, 29, kNoArea) != Due::Drop) Fail("another area does not drop");
    if (Decide(2, 29, 29, 30) != Due::Drop) Fail("a came-from word already set does not drop");
    bof3::Log("shadow      save tail self-test: zero tail no tag; the place written and read back, the rest zero; a "
              "changed byte, another version, a length past the tail, a record past the records damaged; an unknown "
              "kind skipped; the put-back waits in modes 0 and 1, puts back in the saved area with no came-from, "
              "drops otherwise");
}

}  // namespace

namespace bof3 {

void SaveTail_Write(unsigned char* tail) {
    if (!g_on) return;
    const CameFrom f = Live();
    Build(tail, f);
    Log("save        tail: came from area 0x%X at (0x%X, 0x%X) (DIV draft C)", f.area, static_cast<unsigned>(f.x),
        static_cast<unsigned>(f.z));
}

void SaveTail_Loaded(const unsigned char* staging, std::uint16_t block_area) {
    if (!g_on) return;
    g_armed = false;
    CameFrom f;
    bool has = false;
    const char* why = "";
    const Read r = Parse(staging + kBlockBytes, f, has, why);
    if (r != Read::Tagged) {
        Log("save        tail: %s%s - the came-from place is not known (DIV-0121's hold at an edge)",
            r == Read::Damaged ? "damaged, " : "", why);
        return;
    }
    if (!has || f.area == kNoArea) {
        Log("save        tail: tagged, %s - the save was made where the game came from nowhere (DIV-0121's hold)",
            has ? "came from no area" : "no came-from record");
        return;
    }
    g_from = f;
    g_saved_area = block_area;
    g_armed = true;
    Log("save        tail: came from area 0x%X at (0x%X, 0x%X); put back once area 0x%X is entered", f.area,
        static_cast<unsigned>(f.x), static_cast<unsigned>(f.z), block_area);
}

void SaveTail_Frame() {
    if (!g_armed) return;
    const Due d = Decide(static_cast<unsigned>(Game_Mode), Game_AreaNumber, g_saved_area, AreaW2c_LastArea);
    if (d == Due::Wait) return;
    g_armed = false;
    if (d == Due::Drop) {
        Log("save        tail: not put back at Frame_Counter %u - mode %u, area 0x%X (the save's 0x%X), came-from 0x%X",
            static_cast<unsigned>(Frame_Counter), static_cast<unsigned>(Game_Mode), Game_AreaNumber, g_saved_area,
            AreaW2c_LastArea);
        return;
    }
    AreaW2c_LastArea = g_from.area;
    std::memcpy(At(MoveScript_TintRecords_addr) + kFromX, &g_from.x, 4);
    std::memcpy(At(MoveScript_TintRecords_addr) + kFromZ, &g_from.z, 4);
    Log("save        tail: put back at Frame_Counter %u in area 0x%X (mode %u): came from area 0x%X at (0x%X, 0x%X)",
        static_cast<unsigned>(Frame_Counter), Game_AreaNumber, static_cast<unsigned>(Game_Mode), g_from.area,
        static_cast<unsigned>(g_from.x), static_cast<unsigned>(g_from.z));
}

void SaveTail_Forget() {
    if (!g_armed) return;
    g_armed = false;
    Log("save        tail: the title's menu began again before the loaded game entered its area; not put back");
}

void SaveTail_Inject(bool selftest) {
    if (selftest) SelfTest();
    static const std::uint8_t was = 0, is = 1;
    if (PatchBytes("SaveCameFrom", Low32(&g_on), &was, &is, 1))
        Log("save        the came-from place is written in a save's tail and put back at a load (DIV draft C)");
}

}  // namespace bof3
