// DIV-0182: an area's own .data put back when its DAT is loaded.
//
// Two area blocks write their own .data (D145, docs/known-defects.md):
// Area108_TailPlace moves Area108_Model's flags byte (+3) and the corners of
// its records (the s8 count at +0, the records at the pointer at +4, 0x28
// bytes each) as the cart's push scene plays; Area52_TailBlock sets byte +1
// of the area's first extra-object entry to 0 when a timed scene begins and
// 0xFF when it ends. On the PlayStation the area's overlay is reloaded from
// disc with the area, so every visit starts from the disc's bytes; the port
// links every overlay into one image and the values stay until the exe is
// restarted - a second visit to area 108 (or a reload of a save there,
// issue #149) starts from the last scene's cart.
//
// Here the bytes are copied once at inject, before any scene, and put back
// each time Area_Enter loads the area's DAT - the branch the PlayStation took
// to load the overlay as well. Nothing else of either area is touched.
#include "game/area_data_reload.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kArea108 = 108;
constexpr unsigned kArea52 = 52;
constexpr std::uint32_t kModelRecordBytes = 0x28;   // four corners of three s16, a velocity of three s16
constexpr std::uint32_t kModelRecordsMax = 127;     // the count is a signed byte

std::uint8_t g_on = 0;   // 1 through PatchBytes("AreaDataReload") in AreaDataReload_Inject

std::uint8_t* Mem(std::uint32_t at) { return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(at)); }

// Area 108: the extra-object entries at Area108_Model (8 bytes each: an s8
// quad count, two bytes, a flags byte, a pointer to count records of 0x28) and
// the records each names - entry 0 the 33-quad model the push scene's tail
// moves, entry 1 the cart's 55 faces that the blast (effect kind 0x35,
// EffectKind35_StepPieces) rewrites in place, entry 2 untouched but copied
// with them. The table ends at the first entry whose count or pointer is not
// a model's (the image's fourth entry is other data).
constexpr unsigned kEntriesMax = 4;
constexpr unsigned kEntryBytes = 8;
struct Model {
    std::uint32_t entry;     // the entry's place
    std::uint32_t records;   // its records' place (the pointer the entry holds)
    std::uint32_t bytes;     // count * kModelRecordBytes
};
Model g_models[kEntriesMax];
unsigned g_model_count = 0;
std::uint8_t g_entries[kEntriesMax * kEntryBytes];
std::uint8_t g_records[kEntriesMax * kModelRecordsMax * kModelRecordBytes];
std::uint32_t g_records_len = 0;

// Area 52: byte +1 of the first extra-object entry.
std::uint8_t g_area52_byte = 0;

// A pointer the image holds, a place: inside .data below the entries.
bool InData(std::uint32_t at) { return at >= ::bof3::Img(bof3::kGameDataVa) && at < ::bof3::Img(Area108_Model_addr); }

void Snapshot() {
    std::uint32_t off = 0;
    for (unsigned k = 0; k < kEntriesMax; ++k) {
        const std::uint32_t entry = ::bof3::Img(Area108_Model_addr) + k * kEntryBytes;
        const std::uint8_t* const e = Mem(entry);
        const auto count = static_cast<signed char>(e[0]);
        std::uint32_t records;
        std::memcpy(&records, e + 4, 4);
        if (count <= 0 || !InData(records)) break;
        g_models[k] = {entry, records, static_cast<std::uint32_t>(count) * kModelRecordBytes};
        std::memcpy(g_entries + k * kEntryBytes, e, kEntryBytes);
        std::memcpy(g_records + off, Mem(records), g_models[k].bytes);
        off += g_models[k].bytes;
        g_model_count = k + 1;
    }
    if (g_model_count == 0) bof3::Fatal("DIV-0182: Area108_Model names no model - not the image's shape");
    g_records_len = off;
    g_area52_byte = Mem(::bof3::Img(Area52_ExtraObjects_addr))[1];
}

// True when anything was put back.
bool PutBackArea108() {
    bool moved = false;
    std::uint32_t off = 0;
    for (unsigned k = 0; k < g_model_count; ++k) {
        const Model& m = g_models[k];
        if (std::memcmp(Mem(m.entry), g_entries + k * kEntryBytes, kEntryBytes) != 0 ||
            std::memcmp(Mem(m.records), g_records + off, m.bytes) != 0)
            moved = true;
        std::memcpy(Mem(m.entry), g_entries + k * kEntryBytes, kEntryBytes);
        std::memcpy(Mem(m.records), g_records + off, m.bytes);
        off += m.bytes;
    }
    return moved;
}

}  // namespace

void AreaDataReload_Inject() {
    Snapshot();
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("AreaDataReload", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_on)), &was, &is, 1);
    bof3::Log("DIV-0182    area 108's models and area 52's scene byte put back at the area's load (on unless the line above says OFF); "
              "%u models, %u record bytes from 0x%08X",
              g_model_count, g_records_len, g_models[0].records - (::bof3::ImageBase() - ::bof3::image::kBase));   // its VA
}

void AreaDataReload_OnLoad(unsigned area) {
    if (!g_on) return;
    if (area == kArea108) {
        if (PutBackArea108()) bof3::Log("DIV-0182    area 108's models put back at the area's load");
    } else if (area == kArea52) {
        std::uint8_t* const byte = Mem(::bof3::Img(Area52_ExtraObjects_addr)) + 1;
        if (*byte != g_area52_byte) bof3::Log("DIV-0182    area 52's scene byte %02X put back to %02X at the area's load", *byte, g_area52_byte);
        *byte = g_area52_byte;
    }
}
