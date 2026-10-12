#pragma once

#include <cstdint>

// DIVERGENCE DIV-0015: the in-game Config screen's text from a language
// overlay. See src/game/config_text.cpp and docs/config-screen.md.

// Applies a kind-7 chunk: six row labels, seventeen option records, six
// controller-panel names. Aborts loudly on a payload that does not describe
// exactly that, since every write below is into BOF3.exe's own .data.
void ConfigText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// Moves the screen's own layout for Latin text: the label column right to the
// edge the PlayStation release uses, and every row's text down two pixels.
// Only with BOF3X_LANG set. See config_text.cpp.
void ConfigText_Inject();

// What ConfigText_Inject and ConfigText_Apply put into the Config screen's
// code (DIV-0015, DIV-0017, DIV-0026, DIV-0051), as values of ours
// (docs/operand-store.md): our Config_DrawRowLabel (field_c1.cpp),
// Config_DrawRowOptions, Config_DrawControllerPanel and
// Config_DrawControllerRow (rest_4f.cpp) read these, never the code; hosted,
// Operands_Check compares each with the patched code. The original's until
// a patch goes in - and for good with no Latin overlay, or under
// BOF3X_ORIGINAL=ConfigText / ConfigController.
struct ConfigTextOperands {
    std::uint32_t label[6];     // 0x461832 + 8 r: the row labels' addresses (DIV-0015, when FIRST.DAT's chunk arrives)
    int anchor;                 // 0x46189D, 0x4618ED: the labels' right edge, 0x3A; 0x5F under the Latin layout
    bool label_x4;              // 0x461894: the selected label's width len * 6, or DIV-0017's len * 4
    bool option_x8;             // 0x4619E1: the selected option's width count * 12, or DIV-0017's * 8
    std::uint32_t big_draw;     // 0x46189F, 0x4619F9: Text_DrawAt 0x516B30, or DIV-0017's ConfigText_DrawSelected
    std::uint32_t name_draw;    // 0x461B43: Text_DrawAt, or DIV-0026's ConfigText_DrawSelected
    bool name_x4;               // 0x461B36: a controller name's width len * 6, or DIV-0026's len * 4
    int name_edge;              // 0x461B41: the names' right edge, 0x20; DIV-0026's 0x36
    unsigned box_width;         // 0x461B07: the row's dark box, 0x68; DIV-0051's 0x58
    bool second_cell;           // 0x461BAC: the row's second cell drawn; not under DIV-0051
    int frame_w;                // 0x461A62: the controller frame's cells across, 0xC; DIV-0051's 0xD
};
const ConfigTextOperands& ConfigText_Operands();
