// DIVERGENCE DIV-0008: item and ability names from a language overlay.
//
// The names the menus draw are not in any DAT: they are fixed-stride record
// tables in BOF3.exe's .data (symbols.toml, NameTable_*), each record a
// 16-byte name field beside numeric fields - the PSX GAME.EMI tables with the
// name widened from 8 bytes (12 on the Western discs). A DAT chunk of kind 5,
// which is ours, carries `count` names of 16 bytes for one table; its tag is
// the address of record 0's name field. Only the name fields are written.
//
// The tag is checked against the six tables below and the size against the
// table's count: an overlay is a local file, but this is a write into the
// image by address, and nothing else may be reachable through it.
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
        if (t.first_name != tag) continue;
        if (size != t.count * kNameBytes)
            bof3::Fatal("name chunk for %s is 0x%X bytes, the table wants 0x%X", t.what, (unsigned)size,
                        (unsigned)(t.count * kNameBytes));
        auto* field = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(t.first_name));
        for (std::uint32_t i = 0; i < t.count; ++i, field += t.stride, names += kNameBytes)
            std::memcpy(field, names, kNameBytes);
        bof3::Log("DIV-0008: %u %s names replaced", (unsigned)t.count, t.what);
        return;
    }
    bof3::Fatal("name chunk with tag 0x%08X: not one of the name tables", (unsigned)tag);
}
