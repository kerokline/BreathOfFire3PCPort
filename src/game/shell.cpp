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
// start-up fuzz can stand recorders in for the callees. Two of the seven keep
// the original's stack in naked asm, because what they leave depends on it:
// Cfg_Load hands Cfg_SetKeyTable 0x80 bytes that run on past its own frame
// into the return address and its caller's, and Game_Init ends in a jmp to
// Task_SetStackBase, which hangs the task stacks from the esp it arrives with.
#include "game/shell.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/shell_callees.h"
#include "hook/detour.h"

// A C symbol as the assembler sees it (i686 mingw prefixes an underscore).
#define SHELL_STR2(x) #x
#define SHELL_STR(x) SHELL_STR2(x)
#define SHELL_SYM(name) SHELL_STR(__USER_LABEL_PREFIX__) #name

namespace shell {

namespace {

template <class To, class From>
To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<void*>(f)); }

}  // namespace

const Callees kOriginals = {
    Pad_Read,
    Crt_fopen, Crt_fgets, Crt_atoi, Cast<int (__cdecl*)(const char*, const char*, void*, void*)>(Crt_sscanf), Crt_fclose,
    Cfg_SetKeyTable, Cfg_SetDefaultKeys,
    Cast<int (__cdecl*)(char*, const char*, const char*, const char*)>(Crt_sprintf), kImportGetDriveType,
    Gte_InitGeom, Gte_SetGeomOffset, Gte_SetGeomScreen, DInput_Init,
    reinterpret_cast<void (__cdecl*)(void*)>(static_cast<std::uintptr_t>(kSoundSetup)), Port_DroppedCall,
    Gpu_SetDefDrawEnv, Gpu_SetDefDispEnv, Gfx_InitBufferBlock, Gfx_BeginFrame, Task_SetStackBase,
    Gpu_ClearOTagR, Gpu_AddPrim,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
const char* Str(U address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }
unsigned long* Longs(U address) { return reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

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
extern "C" void __cdecl Shell_CfgLoadFrame(unsigned char* frame) {
    void* const file = g.open(Str(kCfgName), Str(kModeRt));
    if (file == nullptr) {
        Cfg_Fullscreen = 1;
        Cfg_RenderMode = 1;
        g.default_keys();
        return;
    }
    char* const line = reinterpret_cast<char*>(frame);
    int n = 0;
    while (g.gets(line, kCfgLineBytes, file) != nullptr) {
        if (n == 0)
            Cfg_Fullscreen = g.to_int(line);
        else if (n == 1)
            Cfg_RenderMode = g.to_int(line);
        else
            g.scan_pair(line, Str(kPairFormat), frame + kCfgPairs + 2 * n, frame + kCfgPairs + 1 + 2 * n);
        ++n;
    }
    g.close(file);
    if (n > 2)
        g.set_key_table(frame + kCfgTable);
    else
        g.default_keys();
}

// original 0x4FD030; sole caller WinMain (0x4FCBFE; ours since) before the window.
// The original's frame - 0x3C bytes, edi and esi saved under it - laid out
// here so that what Cfg_SetKeyTable copies past the frame is what it was:
// the return address and the caller's bytes above it.
extern "C" __attribute__((naked)) void __cdecl Cfg_Load(void) {
    asm("subl $0x3C, %esp\n\t"
        "pushl %edi\n\t"
        "pushl %esi\n\t"
        "leal 8(%esp), %eax\n\t"
        "pushl %eax\n\t"
        "call " SHELL_SYM(Shell_CfgLoadFrame) "\n\t"
        "addl $4, %esp\n\t"
        "popl %esi\n\t"
        "popl %edi\n\t"
        "addl $0x3C, %esp\n\t"
        "ret");
}

// original 0x5A9880; sole caller Cfg_Load. The 32 dwords of Key_TableDefault
// to Key_Table, first to last (rep movsd; the two do not overlap).
extern "C" void __cdecl Cfg_SetDefaultKeys(void) {
    for (U i = 0; i < 0x20; ++i) PutLong(kKeyTable + 4 * i, Long(kKeyTableDefault + 4 * i));
}

// original 0x4FC6A0; called once a pass of WinMain's message loop
// (InputScript_Latch's DeviceLatch, or Capcom's WinMain at 0x4FCDDE). Pad_Read's
// low word is pad 1: Input_Previous gets Input_Held as it stood after the
// read, Input_Held the new word, Input_Pressed the bits that went down. The
// high word - which Pad_Read never sets - the same for pad 2 at 0x7E1BF0.
extern "C" void __cdecl Input_Latch(void) {
    const U word = g.pad_read();
    const std::uint16_t now1 = static_cast<std::uint16_t>(word);
    const std::uint16_t was1 = Input_Held;
    Input_Held = now1;
    Input_Previous = was1;
    Input_Pressed = static_cast<std::uint16_t>((was1 ^ now1) & now1);
    const std::uint16_t now2 = static_cast<std::uint16_t>(word >> 16);
    const std::uint16_t was2 = Input2_Held;
    Input2_Previous = was2;
    Input2_Held = now2;
    Input2_Pressed = static_cast<std::uint16_t>((was2 ^ now2) & now2);
}

// original 0x5A72C0; sole caller WinMain (ours), with CAPCOM.AVI and BOF3.EXE.
// The local file opens: closed again, the root emptied (byte 0 of
// File_CdRootBuf to 0, so File_Open's paths are relative), 1. Otherwise each
// drive letter C..L in turn goes into byte 0 of the root "C:\", and on a
// CD-ROM drive (GetDriveTypeA, read from its import slot once, before the
// loop) the disc file is tried at root + name: found, closed, 1, the root
// left naming the drive. None: 0, the root left at L. The path is built in a
// 0x50-byte buffer as the original's; WinMain's two names fit it.
extern "C" int __cdecl Disc_Probe(const char* local_file, const char* disc_file) {
    void* const local = g.open(local_file, Str(kModeRb));
    if (local != nullptr) {
        g.close(local);
        At(kCdRoot)[0] = 0;
        return 1;
    }
    DriveTypeFn drive_type;
    std::memcpy(&drive_type, At(g.drive_type_slot), sizeof drive_type);
    char path[0x50];
    for (U i = 0; i < 10; ++i) {
        At(kCdRoot)[0] = static_cast<unsigned char>('C' + i);
        if (drive_type(Str(kCdRoot)) != kDriveCdrom) continue;
        g.format(path, Str(kRootFormat), Str(kCdRoot), disc_file);
        void* const disc = g.open(path, Str(kModeRb));
        if (disc != nullptr) {
            g.close(disc);
            return 1;
        }
    }
    return 0;
}

// Game_Init's body. Returns where the entry below jumps: Task_SetStackBase.
extern "C" void* __cdecl Shell_GameInitBody(void) {
    g.init_geom();
    g.set_geom_offset(0xA0, 0x78);
    g.set_geom_screen(1000);
    g.dinput_init(Game_HInstance, Game_Hwnd);
    g.sound_setup(Game_Hwnd);   // read again: DInput_Init may have changed it
    g.dropped_call(0);
    unsigned char* const block0 = At(kBlocks);
    unsigned char* const block1 = At(kBlocks + kBlockBytes);
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

// original 0x4FD110; sole caller WinMain after Display_Setup. The GTE's
// geometry, the pad devices, the sound set-up, the two double-buffer blocks
// (block 0 draws at y 0 and shows y 240, block 1 the other way round), buffer
// 0 current, the frame begun, the quit flag down - and then a jmp, not a call,
// to Task_SetStackBase, so that the task stacks hang from the esp this was
// entered with: the body runs as a call, and the jmp is made from here.
extern "C" __attribute__((naked)) void __cdecl Game_Init(void) {
    asm("call " SHELL_SYM(Shell_GameInitBody) "\n\t"
        "jmp *%eax");
}

// original 0x4FD200; Game_Init's, once for each block. The block's ordering
// table cleared (ClearOTagR, eight slots at +0x70), then its DRAWENV's isbg
// byte (+0x2C) 1 and clear colour (+0x2D..+0x2F) black. The eax the original
// leaves (Gpu_ClearOTagR's, al 0) is not read by its caller.
extern "C" void __cdecl Gfx_InitBufferBlock(unsigned char* block) {
    g.clear_otag_r(reinterpret_cast<unsigned long*>(block + kBlockOt), kOtSlots);
    block[0x2C] = 1;
    block[0x2D] = 0;
    block[0x2E] = 0;
    block[0x2F] = 0;
}

// original 0x4FD290; WinMain once a frame, after the frame's logic. For slot
// i of 8: Gpu_AddPrim(the current block's slot i, the list head Gfx_OtHeads[
// Gfx_BufferIndex * 8 + i], Gfx_OtPointers[i]) - the head spliced onto the
// table, the table's old link stored where the frame's packets of that slot
// end. Gfx_OtPointers[i], the buffer index (a byte) and Gfx_CurrentEnv are all
// read afresh each pass, after the call before.
extern "C" void __cdecl Gfx_LinkOTags(void) {
    for (U i = 0; i < kOtSlots; ++i) {
        const U link = Long(kGfxOtPointers + 4 * i);
        const U buffer = Gfx_BufferIndex;
        const U block = Addr(Gfx_CurrentEnv);
        g.add_prim(Longs(block + kBlockOt + 4 * i), Longs(kGfxOtHeads + (i + buffer * 8) * 4), Longs(link));
    }
}

void Shell_Inject() {
    if (bof3::WantsShadow("shell")) shell::SelfTest();
    BOF3_INJECT(Input_Latch);
    BOF3_INJECT(Cfg_Load);
    BOF3_INJECT(Game_Init);
    BOF3_INJECT(Gfx_InitBufferBlock);
    BOF3_INJECT(Gfx_LinkOTags);
    BOF3_INJECT(Disc_Probe);
    BOF3_INJECT(Cfg_SetDefaultKeys);
}
