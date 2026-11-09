// The seven "Windows shell" starts of the platform read pass, which are game
// code filed by their address - group PW of the platform round
// (docs/platform-read-pass.md section 2, docs/platform-layers-plan.md section
// 4 step 2). Each read to its last instruction with capstone against
// bof3/BOF3.exe, 2026-10-05 (docs/shell.md):
//
//   Input_Latch          0x4FC6A0  0x4E    Gfx_InitBufferBlock  0x4FD200  0x24
//   Cfg_Load             0x4FD030  0xD4    Gfx_LinkOTags        0x4FD290  0x4A
//   Game_Init            0x4FD110  0xE2    Disc_Probe           0x5A72C0  0xB0
//   Cfg_SetDefaultKeys   0x5A9880  0x16
//
// Faithful: no divergence. Every call out goes through shell::g, so the
// start-up fuzz can stand recorders in for the callees. Cfg_Load keeps the
// original's stack in naked asm, because what it leaves depends on it: it
// hands Cfg_SetKeyTable 0x80 bytes that run on past its own frame into the
// return address and its caller's (a pair by width since the 64-bit build
// round). Game_Init ends in a jump to Task_SetStackBase, which hangs the
// task stacks from the stack pointer it arrives with: a C++ tail call.
#include "game/shell.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/shell_callees.h"
#include "hook/detour.h"
#include "hook/exe_host.h"
#include "hook/host.h"
#include "hook/log.h"
#include "input/bindings.h"

namespace shell {

namespace {

template <class To, class From>
To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<void*>(f)); }

}  // namespace

const Callees kOriginals = {
    Pad_Read,
    Crt_fopen, Crt_fgets, Crt_atoi, Cast<int (__cdecl*)(const char*, const char*, void*, void*)>(Crt_sscanf), Crt_fclose,
    Cfg_SetKeyTable, Cfg_SetDefaultKeys,
    Cast<int (__cdecl*)(char*, const char*, const char*, const char*)>(Crt_sprintf), ::bof3::Img(Shell_ImportGetDriveType_addr),
    Gte_InitGeom, Gte_SetGeomOffset, Gte_SetGeomScreen, DInput_Init,
    reinterpret_cast<void (__cdecl*)(void*)>(static_cast<std::uintptr_t>(::bof3::Code(kSoundSetup.v))), Port_DroppedCall,
    Gpu_SetDefDrawEnv, Gpu_SetDefDispEnv, Gfx_InitBufferBlock, Gfx_BeginFrame, Task_SetStackBase,
    Gpu_ClearOTagR, Gpu_AddPrim,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
const char* Str(U address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }

}  // namespace
}  // namespace shell

using namespace shell;

// Cfg_Load's body, on the frame the naked entry below laid out exactly as the
// original's: `frame` is the 0x3C bytes under the return address. Line 0 and
// line 1 through atoi to Cfg_Fullscreen and Cfg_RenderMode; line n >= 2 through
// sscanf("%d %d") as two ints to frame + 0x10 + 2n and + 0x11 + 2n - byte
// pointers, so each int lands one byte above the last and the pair keeps only
// the low byte of the first and the first byte of the second, the second's
// other three bytes running into the next pair (or, from the 21st line, the
// return address: a latent defect, docs/shell.md section 3). A line whose
// integers do not scan leaves those bytes as they were. More than two lines:
// Cfg_SetKeyTable(frame + 0x14), 0x80 bytes of which this frame holds 0x28.
// No file: both settings 1. Otherwise, two lines or fewer: the default keys.
// fgets 0x14 at a time, so a longer line counts as several.
//
// DIVERGENCE DIV-0078 (the owner, 2026-10-06): with g_cfg_own_table on, the
// key lines scan into a zero-filled 0x80-byte table of our own (plus the 8
// bytes the last pair's ints spill into), through the same byte pointers and
// the same packing - two lines an entry, the spill as the original leaves it -
// and lines past the 32nd entry (the 66th line on) are read and ignored; the
// table copied is ours, never the frame's run past its end. The switch is set
// by Shell_Inject after the self-test, which compares Capcom's frame.
unsigned char g_cfg_own_table = 0;
alignas(4) unsigned char g_cfg_table[0x80 + 8];
extern "C" void __cdecl Shell_CfgLoadFrame(unsigned char* frame) {
    void* const file = g.open(Str(::bof3::Img(kCfgName.v)), Str(::bof3::Img(kModeRt.v)));
    if (file == nullptr) {
        Cfg_Fullscreen = 1;
        Cfg_RenderMode = 1;
        g.default_keys();
        return;
    }
    const bool own = g_cfg_own_table != 0;
    if (own) std::memset(g_cfg_table, 0, sizeof g_cfg_table);
    char* const line = reinterpret_cast<char*>(frame);
    int n = 0;
    while (g.gets(line, kCfgLineBytes, file) != nullptr) {
        if (n == 0)
            Cfg_Fullscreen = g.to_int(line);
        else if (n == 1)
            Cfg_RenderMode = g.to_int(line);
        else if (!own)
            g.scan_pair(line, Str(::bof3::Img(kPairFormat.v)), frame + kCfgPairs + 2 * n, frame + kCfgPairs + 1 + 2 * n);
        else if (n - 2 < 0x40)   // DIV-0078: entry (n - 2) / 2 of 32; later lines ignored
            g.scan_pair(line, Str(::bof3::Img(kPairFormat.v)), g_cfg_table + 2 * (n - 2), g_cfg_table + 1 + 2 * (n - 2));
        ++n;
    }
    g.close(file);
    if (n > 2)
        g.set_key_table(own ? g_cfg_table : frame + kCfgTable);
    else
        g.default_keys();
}

// Key_TableDefault 0x66C648 written from the engine's rule at map time, so
// that Capcom's code paths that read it (hosted, under BOF3X_ORIGINAL, and the
// shell fuzz's clone) see it too; first compared with BOF3.exe's 0x80 bytes -
// the mapped image's hosted, the player's file in the exe host - when there
// is one. The region is then the engine's own (docs/disc-only-round.md, RD).
void Shell_MapDefaultKeys() {
    unsigned char rule[0x80], theirs[0x80];
    bof3x::input::DefaultKeyTable(rule, sizeof rule);
    const bool present = bof3::ExeFile_Present();
    if (present) {
        if (bof3::IsExeHost()) bof3::ExeFile_Read(Key_TableDefault_addr, theirs, sizeof theirs);
        else std::memcpy(theirs, At(::bof3::Img(Key_TableDefault_addr)), sizeof theirs);
        for (unsigned i = 0; i < sizeof rule; i += 4)
            if (std::memcmp(rule + i, theirs + i, 4) != 0)
                bof3::Fatal("keys: the default key table's entry %u is key 0x%02X%02X bits 0x%02X%02X by the engine's "
                            "rule, key 0x%02X%02X bits 0x%02X%02X in BOF3.exe at 0x%06X", i / 4, rule[i + 1], rule[i],
                            rule[i + 3], rule[i + 2], theirs[i + 1], theirs[i], theirs[i + 3], theirs[i + 2],
                            (unsigned)(Key_TableDefault_addr + i));
    }
    std::memcpy(At(::bof3::Img(Key_TableDefault_addr)), rule, sizeof rule);
    bof3::Log("keys: Key_TableDefault 0x%06X written from the engine's rule (%u keys)%s", (unsigned)Key_TableDefault_addr,
              (unsigned)bof3x::input::DefaultKeys().size(),
              present ? ", equal to BOF3.exe's" : "; no BOF3.exe to compare it with");
}

// Game_Init's body. Returns where the entry below jumps: Task_SetStackBase.
extern "C" void* __cdecl Shell_GameInitBody(void) {
    g.init_geom();
    g.set_geom_offset(0xA0, 0x78);
    g.set_geom_screen(1000);
    g.dinput_init(Game_HInstance, Game_Hwnd);
    g.sound_setup(Game_Hwnd);   // read again: DInput_Init may have changed it
    g.dropped_call(0);
    unsigned char* const block0 = At(::bof3::Img(kBlocks));
    unsigned char* const block1 = At(::bof3::Img(kBlocks) + kBlockBytes);
    g.set_def_draw_env(block0 + kBlockDrawEnv, 0, 0, 0x140, 0xF0);
    g.set_def_disp_env(block0, 0, 0xF0, 0x140, 0xF0);
    g.set_def_draw_env(block1 + kBlockDrawEnv, 0, 0xF0, 0x140, 0xF0);
    g.set_def_disp_env(block1, 0, 0, 0x140, 0xF0);
    g.init_block(block0);
    g.init_block(block1);
    Gfx_BufferIndex = 0;
    Gfx_CurrentEnv = block0;
    g.begin_frame();
    Game_QuitFlag = 0;
    return reinterpret_cast<void*>(g.set_stack_base);
}

void Shell_Inject() {
    if (!bof3::IsExeHost()) Shell_MapDefaultKeys();   // the exe host's is at map time (exe_host.cpp step 3)
    if (bof3::WantsShadow("shell")) shell::SelfTest();
    // DIVERGENCE DIV-0078: after the self-test, which compares Capcom's frame.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("CfgOwnKeyTable", Low32(&g_cfg_own_table),
                         &was, &is, 1);
        bof3::Log("DIV-0078    BOF3.CFG's key lines scan into a 32-entry table of our own; lines past it are ignored");
    }
}
