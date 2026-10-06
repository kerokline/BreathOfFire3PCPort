// Internal to shell.cpp and shell_fuzz.cpp: the addresses the seven "Windows
// shell" starts touch, and every call they make - through pointers, so that
// the start-up fuzz can stand recording functions in for them, for Capcom's
// copies and for ours alike. docs/shell.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace shell {

using U = std::uint32_t;

// --- Data, all Capcom's addresses (docs/shell.md section 1) ------------------
constexpr U kCfgName = 0x65DAD8;      // the BOF3.CFG literal Cfg_Load opens
constexpr U kModeRt = 0x65DAE4;       // its mode literal, text read
constexpr U kPairFormat = 0x65DAD0;   // the two-integer sscanf format of lines 3 on
constexpr U kModeRb = 0x66BC38;       // Disc_Probe's mode literal, binary read
constexpr U kRootFormat = 0x66BC30;   // Disc_Probe's two-string sprintf format
constexpr U kCdRoot = 0x66BC2C;       // File_CdRootBuf, the drive root File_Open prepends
// The globals symbols.gen.h binds as typed lvalues (so their bof3::addr
// constants are hidden behind the macros), by address for the fuzz's regions
// and the dword copies.
constexpr U kKeyTable = 0x7DE7A8;          // Key_Table, 32 (scancode, bits) pairs
constexpr U kKeyTableDefault = 0x66C648;   // Key_TableDefault, the same shape
constexpr U kGameHwnd = 0x6BC620;          // Game_Hwnd
constexpr U kGameHInstance = 0x6BC624;     // Game_HInstance
constexpr U kGameQuitFlag = 0x6BC639;      // Game_QuitFlag, a byte
constexpr U kGfxBufferIndex = 0x905B89;    // Gfx_BufferIndex, a byte
constexpr U kGfxCurrentEnv = 0x937F84;     // Gfx_CurrentEnv, the current block
constexpr U kGfxOtPointers = 0x929EA0;     // Gfx_OtPointers, 8 dwords
constexpr U kGfxOtHeads = 0x9037C0;        // Gfx_OtHeads, 8 list heads per buffer
constexpr U kBlocks = bof3::addr::Gfx_BufferBlocks;  // 0x903880, the two double-buffer blocks
constexpr U kBlockBytes = 0x90;       // DISPENV +0, DRAWENV +0x14, the ordering table +0x70
constexpr U kBlockDrawEnv = 0x14;
constexpr U kBlockOt = 0x70;
constexpr U kOtSlots = 8;

// Cfg_Load's frame, as the original lays it out below its return address:
// 0x3C bytes, the line buffer its first 0x14, line n's pair from +0x10 + 2n
// (n >= 2, so the array starts at +0x14), Cfg_SetKeyTable handed +0x14.
constexpr U kCfgFrameBytes = 0x3C;
constexpr U kCfgLineBytes = 0x14;
constexpr U kCfgPairs = 0x10;
constexpr U kCfgTable = 0x14;

// The Win32 import slot Disc_Probe calls through (the IAT, by name from the
// import directory 2026-10-05): KERNEL32 GetDriveTypeA.
constexpr U kImportGetDriveType = 0x5C4088;
constexpr U kDriveCdrom = 5;          // DRIVE_CDROM

// The sound set-up Game_Init calls (0x4FD144), not ours and not named
// tonight: the platform round's sound group reads it (docs/shell.md section 6).
constexpr U kSoundSetup = 0x5A6830;

using DriveTypeFn = unsigned (__stdcall*)(const char* root);   // GetDriveTypeA

struct Callees {
    // Input_Latch's
    unsigned (__cdecl* pad_read)();                                            // Pad_Read (ours)
    // Cfg_Load's: the C runtime (Capcom's) and the two key-table copies
    void* (__cdecl* open)(const char* path, const char* mode);                // Crt_fopen
    char* (__cdecl* gets)(char* buf, int n, void* stream);                    // Crt_fgets
    int (__cdecl* to_int)(const char* s);                                        // Crt_atoi
    int (__cdecl* scan_pair)(const char* s, const char* fmt, void* a, void* b);   // Crt_sscanf, as Cfg_Load calls it
    int (__cdecl* close)(void* stream);                                       // Crt_fclose
    void (__cdecl* set_key_table)(const void* table);                          // Cfg_SetKeyTable (ours)
    void (__cdecl* default_keys)();                                            // Cfg_SetDefaultKeys (this module)
    // Disc_Probe's
    int (__cdecl* format)(char* dst, const char* fmt, const char* a, const char* b);   // Crt_sprintf, as called
    U drive_type_slot;   // where GetDriveTypeA is read from, once a call: the import slot 0x5C4088
    // Game_Init's
    void (__cdecl* init_geom)();                                               // Gte_InitGeom
    void (__cdecl* set_geom_offset)(long x, long y);                           // Gte_SetGeomOffset
    void (__cdecl* set_geom_screen)(long h);                                   // Gte_SetGeomScreen
    void (__cdecl* dinput_init)(void* hinstance, void* hwnd);                  // DInput_Init
    void (__cdecl* sound_setup)(void* hwnd);                                   // 0x5A6830, raw
    void (__cdecl* dropped_call)(unsigned char b);                             // Port_DroppedCall
    unsigned char* (__cdecl* set_def_draw_env)(unsigned char* env, int x, int y, int w, int h);   // Gpu_SetDefDrawEnv
    unsigned char* (__cdecl* set_def_disp_env)(unsigned char* env, int x, int y, int w, int h);   // Gpu_SetDefDispEnv
    void (__cdecl* init_block)(unsigned char* block);                          // Gfx_InitBufferBlock (this module)
    void (__cdecl* begin_frame)();                                             // Gfx_BeginFrame
    void (__cdecl* set_stack_base)();                                          // Task_SetStackBase: reached by a jmp
    // Gfx_InitBufferBlock's and Gfx_LinkOTags'
    void (__cdecl* clear_otag_r)(unsigned long* ot, int n);                    // Gpu_ClearOTagR
    void (__cdecl* add_prim)(unsigned long* ot, unsigned long* prim, unsigned long* link_out);   // Gpu_AddPrim
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=shell: the start-up fuzz, shell_fuzz.cpp. Clones every
// original before Shell_Inject patches it.
void SelfTest();

}  // namespace shell
