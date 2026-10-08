// DIVERGENCE DIV-0008: item and ability names from a language overlay.
//
// The names the menus draw are not in any DAT: they are fixed-stride record
// tables in BOF3.exe's .data (symbols.toml, NameTable_*), each record a
// 16-byte name field beside numeric fields - the PSX GAME.EMI tables with the
// name widened from 8 bytes (12 on the Western discs). A DAT chunk of kind 5,
// which is ours, carries names of 16 bytes for one table: its tag is the
// address of the first record's name field, its size 16 times the records it
// names - the whole table (a language overlay, DIV-0008), or since DIV-0086 a
// run of records from any one (an optional layer's renames,
// docs/opt-layers.md). Only the name fields are written.
//
// The tag is checked against the six tables below - on a record's name field -
// and the size against the records left from there: an overlay is a local
// file, but this is a write into the image by address, and nothing else may be
// reachable through it.
#include "game/name_tables.h"

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kNameBytes = 16;

struct Table {
    const char* what;
    std::uint32_t first_name;  // address of record 0's name field
    std::uint32_t stride;
    std::uint32_t count;
};

// Measured 2026-09-20: each table's numeric fields equal the JP disc's for
// every record, and each chains into the next at first + count * stride
// (weapons to armour with four bytes between).
constexpr Table kTables[] = {
    {"consumables", bof3::addr::NameTable_Consumables, 22, 92},
    {"key items", bof3::addr::NameTable_KeyItems, 20, 16},
    {"weapons", bof3::addr::NameTable_Weapons, 28, 83},
    {"armour", bof3::addr::NameTable_Armour, 26, 68},
    {"accessories", bof3::addr::NameTable_Accessories, 24, 52},
    {"abilities", bof3::addr::NameTable_Abilities + 8, 24, 227},  // 8 parameter bytes, then the name
};

}  // namespace

void NameTables_Apply(std::uint32_t tag, const std::uint8_t* names, std::uint32_t size) {
    for (const Table& t : kTables) {
        if (tag < t.first_name || tag >= t.first_name + t.count * t.stride) continue;
        if ((tag - t.first_name) % t.stride)
            bof3::Fatal("name chunk with tag 0x%08X: inside the %s table, not on a name field", (unsigned)tag, t.what);
        const std::uint32_t first = (tag - t.first_name) / t.stride;
        const std::uint32_t n = size / kNameBytes;
        if (size == 0 || size % kNameBytes || first + n > t.count)
            bof3::Fatal("name chunk for %s from record %u is 0x%X bytes; 0x%X are left", t.what, (unsigned)first,
                        (unsigned)size, (unsigned)((t.count - first) * kNameBytes));
        auto* field = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(tag));
        for (std::uint32_t i = 0; i < n; ++i, field += t.stride, names += kNameBytes)
            std::memcpy(field, names, kNameBytes);
        if (n == t.count)
            bof3::Log("DIV-0008: %u %s names replaced", (unsigned)t.count, t.what);
        else
            bof3::Log("DIV-0086: %u %s name(s) replaced from record %u", (unsigned)n, t.what, (unsigned)first);
        return;
    }
    bof3::Fatal("name chunk with tag 0x%08X: not one of the name tables", (unsigned)tag);
}
