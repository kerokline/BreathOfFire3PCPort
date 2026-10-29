// The raw addresses effect_6b.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_6b.md.
// The callees owned by another group of this round (E6A, E6C, wave six) and
// two of Capcom's in no group are called by these raw addresses until the
// round's rebinding pass; the rest are cells and the image's read-only tables
// the states index (read in place, never copied).
#pragma once

#include <cstdint>
#include "bof3/symbols.gen.h"  // round thirteen's rebinding (docs/round-13-cleanup.md): the targets that are ours read bof3::addr::<Name>, the values unchanged, so the fuzz keys stand

namespace effect_6b::at {

// --- callees not ours ---------------------------------------------------------------
constexpr ::bof3::image::CodeVa kE6ADraw{bof3::addr::EffectKind18Sub2F_Draw};        // E6A (wave six): void(void), the panel draw sub-kinds 0x33,
                                                    // 0x34 and 0x37 call or tail-jump to (EffectKind18Sub37_States[3])
constexpr ::bof3::image::CodeVa kE6CStep{bof3::addr::EffectKind18Sub44_Follow};        // E6C (wave six): void(void), called by EffectKind18Sub44_Step
constexpr ::bof3::image::CodeVa kE6CTail{bof3::addr::EffectKind18Sub44_Draw};        // E6C (wave six): void(void), EffectKind18Sub44_Step's tail jump
constexpr ::bof3::image::CodeVa kWaveMark{bof3::addr::EffectKind18Sub41_DrawPanels};   // R3G's: (dword, byte) - sub-kind 0x41's
                                                    // fourth state, while story flag 0x4F is set and +0xA < 0x19
constexpr ::bof3::image::CodeVa kWaveStep{bof3::addr::EffectKind18Sub41_DrawRings};    // R3G's: (byte +0xA) - the same state

// --- the leader (ObjTrio record 0) and the map cells --------------------------------
// kLeaderX: now ObjTrio[0].x (symbols.toml). ObjTrio +0x34: the leader's x, 16.16
// kLeaderZ: now ObjTrio[0].z (symbols.toml). ObjTrio +0x38: its z, 16.16
// kLeaderHeight: now ObjTrio[0].height_whole (symbols.toml). ObjTrio +0x3E: the word sub-kind 0x3D sets from the ground
constexpr ::bof3::image::Va kLeaderCellZ{0x905E62};    // the high word of Field_Kind2Z (s16): sub-kind 0x3D's row
constexpr ::bof3::image::Va kLeaderCellX{0x905E66};    // the high word of Field_Kind2X (s16): its column
// kStoryFlags: now Cond_Flags[0xa0] (symbols.toml). the story flag row Flags_Test / Flags_Set are handed
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). the chapter's step byte (beside MoveScript_Var7 0x8034E4)

// --- the panels' variant tables, indexed by the spawn's x cell (+0x36, movsx) -------
// Two bytes a variant (the cell x, z put into +0x36, +0x3A). The room is the
// run of bytes to the next datum: two variants before a state table or a
// pair of s8 (+1, -1) that no code reads (pe_xref), four where the run holds
// eight bytes (the fourth variant all zero). Ours aborts past the room.
constexpr ::bof3::image::Va kSub33Cells{0x65EBD4};     // sub-kind 0x33: room 2 (EffectKind18Sub34_States follows)
constexpr ::bof3::image::Va kSub34Cells{0x65EBEC};     // sub-kind 0x34: room 2 (an s8 pair at 0x65EBF0)
constexpr ::bof3::image::Va kSub37Cells{0x65EC04};     // sub-kind 0x37: room 2 (EffectKind18Sub35_States follows)
constexpr ::bof3::image::Va kSub35Cells{0x65EC1C};     // sub-kind 0x35: room 4 (an s8 pair at 0x65EC24)
constexpr ::bof3::image::Va kSub38Cells{0x65EC38};     // sub-kind 0x38: room 4 (EffectKind18Sub3B_States follows)
constexpr ::bof3::image::Va kSub3BHeights{0x65EC54};   // sub-kind 0x3B: s16 each, the lift into +0x3E (room 2)
constexpr ::bof3::image::Va kSub3BCells{0x65EC58};     // sub-kind 0x3B: room 2 (its draw's signs follow)
constexpr unsigned kRoom2 = 2;
constexpr unsigned kRoom4 = 4;
constexpr ::bof3::image::Va kSub3BSides{0x65EC5C};     // two s8: sub-kind 0x3B's panels' slide signs (room four)
constexpr unsigned kSidesRoom = 4;
constexpr ::bof3::image::Va kSub54Blues{0x65EC60};     // four bytes: sub-kind 0x54's fill's blue (<< 3), indexed by +2
constexpr unsigned kSub54Steps = 4;

// --- sub-kind 0x40's panes -------------------------------------------------------
constexpr ::bof3::image::Va kSub40Panes{0x65EC74};     // three of four bytes: x, z, width, depth (cells of 32 units)
constexpr unsigned kSub40PaneCount = 3;

// --- sub-kind 0x41 -------------------------------------------------------------------
constexpr ::bof3::image::Va kSub41Curve{0x65EC80};     // bytes by +9: the rise (<< 4 the draw's angle); room 0x1C
constexpr unsigned kSub41CurveRoom = 0x1C;
constexpr ::bof3::image::Va kSub41MarkA{0x65ECAC};     // bytes by +0xA (< 0x19): << 2 kWaveMark's first word
constexpr ::bof3::image::Va kSub41MarkB{0x65ECC8};     // bytes by +0xA >> 1: kWaveMark's second word
constexpr ::bof3::image::Va kSub41Quads{0x65ECD8};     // four quads of four vertices of three bytes (x, z, -y; << 7)
constexpr unsigned kSub41QuadCount = 4;
constexpr ::bof3::image::Va kSub41Order{0x65ED08};     // four quad indexes a variant (two variants)
constexpr unsigned kSub41Variants = 2;
constexpr ::bof3::image::Va kSub41Textures{0x65ED10};  // dword by quad: the texture word
constexpr ::bof3::image::Va kSub41Turns{0x65ED20};     // s8 by quad: times the angle, the texture's rotation bits

// --- sound ids ------------------------------------------------------------------------
constexpr unsigned kSoundOpen = 0x200;              // the panels: the leader comes near (sub-kind 0x41: flag 0x4F set)
constexpr unsigned kSoundShut = 0x201;              // the panels: closed again (sub-kind 0x41: flag 0x4F clear)
constexpr unsigned kSoundCue = 0x202;               // sub-kind 0x40: the chapter's run 0xC at step 2
constexpr unsigned kSoundEnd = 0x203;               // sub-kind 0x40's third state, after 0x206
constexpr unsigned kSoundEndA = 0x206;

}  // namespace effect_6b::at
