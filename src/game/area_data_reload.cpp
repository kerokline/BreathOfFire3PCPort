// DIV-0181: an area's own .data put back when its DAT is loaded.
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

// Area 108: the model's header (count, two bytes, the flags byte, the pointer)
// and the records the pointer names.
std::uint8_t g_model_header[8];
std::uint8_t g_model_records[kModelRecordsMax * kModelRecordBytes];
std::uint32_t g_model_records_at = 0;
std::uint32_t g_model_records_len = 0;

// Area 52: byte +1 of the first extra-object entry.
std::uint8_t g_area52_byte = 0;

void Snapshot() {
    const std::uint8_t* const model = Mem(Area108_Model_addr);
    std::memcpy(g_model_header, model, sizeof g_model_header);
    const auto count = static_cast<signed char>(model[0]);
    std::uint32_t records;
    std::memcpy(&records, model + 4, 4);
    if (count <= 0 || records < bof3::kGameDataVa) {
        bof3::Fatal("DIV-0181: Area108_Model has count %d and records at 0x%08X - not the image's shape", count, records);
    }
    g_model_records_at = records;
    g_model_records_len = static_cast<std::uint32_t>(count) * kModelRecordBytes;
    std::memcpy(g_model_records, Mem(records), g_model_records_len);
    g_area52_byte = Mem(Area52_ExtraObjects_addr)[1];
}

}  // namespace

void AreaDataReload_Inject() {
    Snapshot();
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("AreaDataReload", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_on)), &was, &is, 1);
    bof3::Log("DIV-0181    area 108's cart model and area 52's scene byte put back at the area's load (on unless the line above says OFF); "
              "%u model records of %u bytes at 0x%08X",
              static_cast<unsigned>(g_model_records_len / kModelRecordBytes), static_cast<unsigned>(kModelRecordBytes), g_model_records_at);
}

void AreaDataReload_OnLoad(unsigned area) {
    if (!g_on) return;
    if (area == kArea108) {
        std::uint8_t* const model = Mem(Area108_Model_addr);
        const bool moved = std::memcmp(model, g_model_header, sizeof g_model_header) != 0 ||
                           std::memcmp(Mem(g_model_records_at), g_model_records, g_model_records_len) != 0;
        std::memcpy(model, g_model_header, sizeof g_model_header);
        std::memcpy(Mem(g_model_records_at), g_model_records, g_model_records_len);
        if (moved) bof3::Log("DIV-0181    area 108's cart model put back at the area's load");
    } else if (area == kArea52) {
        std::uint8_t* const byte = Mem(Area52_ExtraObjects_addr) + 1;
        if (*byte != g_area52_byte) bof3::Log("DIV-0181    area 52's scene byte %02X put back to %02X at the area's load", *byte, g_area52_byte);
        *byte = g_area52_byte;
    }
}
