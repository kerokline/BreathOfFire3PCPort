// Tests of the operating-system seam (src/os/os.h) on the machine they are
// built on: every service the engine uses, with synthetic data only. Built
// with the system compiler on Linux (src/os/posix) and with llvm-mingw on
// Windows (src/os/win32), both in CI. See tools/os_host/CMakeLists.txt.
#include "common/sha256.h"
#include "os/os.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace os = bof3::os;

namespace {

int g_fail = 0;
int g_checks = 0;

void Check(bool ok, const char* what, int line) {
    ++g_checks;
    if (!ok) {
        ++g_fail;
        std::fprintf(stderr, "FAIL line %d: %s\n", line, what);
    }
}
#define CHECK(x) Check((x), #x, __LINE__)

constexpr std::size_t kPage = 0x1000;

// ---- memory -----------------------------------------------------------------

void TestMemory() {
    // Anywhere: allocate, write, protect, restore, release.
    auto* p = static_cast<unsigned char*>(os::MemAlloc(nullptr, 4 * kPage, os::Prot::ReadWrite));
    CHECK(p != nullptr);
    if (!p) return;
    std::memset(p, 0x5A, 4 * kPage);
    os::ProtSave saved;
    CHECK(os::MemProtect(p, kPage, os::Prot::Read, &saved));
    os::MemInfo m;
    CHECK(os::MemQuery(p, &m));
    CHECK(m.state == os::MemState::Committed && m.readable && !m.writable);
    CHECK(os::MemRestore(p, kPage, saved));
    CHECK(os::MemQuery(p, &m) && m.writable);
    CHECK(os::MemProtect(p + kPage, kPage, os::Prot::None));
    CHECK(os::MemQuery(p + kPage, &m) && !m.readable);
    CHECK(os::MemProtect(p + kPage, kPage, os::Prot::ReadWrite));
    CHECK(p[0] == 0x5A && p[kPage] == 0x5A);

    // Execute: a RWX page holding "mov eax, 42; ret" (x86), or "mov w0, 42; ret" (arm64).
    auto* code = static_cast<unsigned char*>(os::MemAlloc(nullptr, kPage, os::Prot::ReadWriteExec));
    CHECK(code != nullptr);
    if (code) {
        CHECK(os::MemQuery(code, &m) && m.executable);
        CHECK(os::MemRelease(code, kPage));
    }
    CHECK(os::MemRelease(p, 4 * kPage));

    // A fixed address: take a free one the system gave, give it back, ask for it by name.
    void* probe = os::MemAlloc(nullptr, 16 * kPage, os::Prot::ReadWrite);
    CHECK(probe != nullptr);
    if (!probe) return;
    CHECK(os::MemRelease(probe, 16 * kPage));
    CHECK(os::MemQuery(probe, &m) && m.state == os::MemState::Free);

    // Reserve at it: no access, then commit part of it, the way the task stacks and the image are made.
    void* at = os::MemReserve(probe, 16 * kPage);
    CHECK(at == probe);
    CHECK(os::MemQuery(probe, &m) && m.state == os::MemState::Reserved && !m.readable);
    // Taken: the same range again must fail, and must not move.
    CHECK(os::MemReserve(probe, 16 * kPage) == nullptr);
    CHECK(os::MemAlloc(probe, kPage, os::Prot::ReadWrite) == nullptr);
    auto* base = static_cast<unsigned char*>(probe);
    CHECK(os::MemCommit(base + 2 * kPage, 3 * kPage, os::Prot::ReadWrite) == base + 2 * kPage);
    base[2 * kPage] = 1;
    base[5 * kPage - 1] = 2;
    CHECK(os::MemQuery(base + 2 * kPage, &m) && m.state == os::MemState::Committed && m.writable);
    // Back to reserved: contents gone.
    CHECK(os::MemDecommit(base + 2 * kPage, 3 * kPage));
    CHECK(os::MemQuery(base + 2 * kPage, &m) && !m.readable);
    CHECK(os::MemCommit(base + 2 * kPage, kPage, os::Prot::ReadWrite) == base + 2 * kPage);
    CHECK(base[2 * kPage] == 0);
    CHECK(os::MemRelease(probe, 16 * kPage));

    // Reserve and commit in one call at a fixed address.
    void* again = os::MemAlloc(probe, 2 * kPage, os::Prot::ReadWrite);
    CHECK(again == probe);
    if (again) {
        std::memset(again, 7, 2 * kPage);
        CHECK(os::MemRelease(again, 2 * kPage));
    }

    // The low address the narrow image wants (0x400000 on Windows). The kernel may refuse a program
    // this range (mmap_min_addr, or its own image there), so the test reports and does not require.
    void* low = os::MemReserve(reinterpret_cast<void*>(0x400000), 0x590000);
    std::printf("os_tests: reserve at 0x400000 for 0x590000 bytes: %s\n", low ? "granted" : "refused");
    if (low) CHECK(os::MemRelease(low, 0x590000));
}

// ---- files ------------------------------------------------------------------

namespace fs = std::filesystem;

std::string Scratch() {
    // A narrow name (os/file.h), as the engine holds one.
    return os::Narrow(fs::temp_directory_path() / ("bof3x_os_tests_" + std::to_string(os::ProcessId())));
}

void Touch(const std::string& path) { std::ofstream(os::Path(path.c_str()), std::ios::binary) << "x"; }

void TestFiles() {
    const std::string dir = Scratch();
    std::error_code ec;
    fs::create_directories(os::Path(dir.c_str()), ec);
    CHECK(os::IsDirectory(dir.c_str()) && !os::IsFile(dir.c_str()));

    Touch(dir + "/a.bin");
    CHECK(os::IsFile((dir + "/a.bin").c_str()) && !os::IsDirectory((dir + "/a.bin").c_str()));
    CHECK(!os::IsFile((dir + "/missing").c_str()));
    // A backslash in the game's own spelling reads as a separator.
    CHECK(os::IsFile((dir + "\\a.bin").c_str()));
    // Bytes above ASCII go through Path and come back the same (TestNarrowNames has the encoding itself).
    const std::string uni = dir + "/caf\xC3\xA9.txt";
    Touch(uni);
    CHECK(os::IsFile(uni.c_str()));
    CHECK(os::Path("DAT\\AREA151.DAT").filename() == fs::path("AREA151.DAT"));

    // Directory walk: a pattern, name order, no dot entries, early stop.
    for (const char* name : {"AREA001.DAT", "AREA010.DAT", "area002.dat", "AREA003.EMI", "OTHER.DAT"}) Touch(dir + "/" + name);
    fs::create_directories(os::Path((dir + "/AREAdir").c_str()), ec);
    std::vector<std::string> seen;
    std::size_t handed = os::WalkDirectory((dir + "/AREA*.DAT").c_str(), [&](const os::DirEntry& e) {
        seen.push_back(e.name);
        return true;
    });
    // FindFirstFile's: ASCII case does not count, in the match or the order.
    const std::vector<std::string> want = {"AREA001.DAT", "area002.dat", "AREA010.DAT"};
    CHECK(handed == want.size() && seen == want);
    seen.clear();
    handed = os::WalkDirectory((dir + "\\*").c_str(), [&](const os::DirEntry& e) {
        seen.push_back(e.name);
        return e.name != "AREA010.DAT";   // stop at it
    });
    CHECK(handed == seen.size() && !seen.empty() && seen.back() == "AREA010.DAT");
    std::vector<std::string> order;
    std::set<std::string> all;
    bool saw_dir = false;
    os::WalkDirectory((dir + "/*.*").c_str(), [&](const os::DirEntry& e) {
        order.push_back(e.name);
        all.insert(e.name);
        if (e.name == "AREAdir") saw_dir = e.is_directory;
        if (e.name == "a.bin") CHECK(e.size == 1);
        return true;
    });
    CHECK(all.count(".") == 0 && all.count("..") == 0 && all.count("a.bin") == 1 && saw_dir);
    // Name order, case-insensitive: a.bin, AREA001.DAT, area002.dat, ...
    CHECK(order.size() >= 3 && order[0] == "a.bin" && order[1] == "AREA001.DAT" && order[2] == "area002.dat");
    // NTFS's order folds to upper case, so '_' (0x5F) sorts after the letters.
    Touch(dir + "/Z_A");
    Touch(dir + "/ZaA");
    order.clear();
    os::WalkDirectory((dir + "/Z*").c_str(), [&](const os::DirEntry& e) {
        order.push_back(e.name);
        return true;
    });
    CHECK(order.size() == 2 && order[0] == "ZaA" && order[1] == "Z_A");
    CHECK(os::WalkDirectory((dir + "/NOPE*").c_str(), [](const os::DirEntry&) { return true; }) == 0);
    CHECK(os::WalkDirectory((dir + "/nodir/*").c_str(), [](const os::DirEntry&) { return true; }) == 0);
    fs::remove_all(os::Path(dir.c_str()), ec);
    CHECK(!os::IsDirectory(dir.c_str()));
}

// ---- the narrow encoding (os/file.h) ------------------------------------------
//
// A `char` name in the engine is in the system's narrow file-name encoding: the
// ANSI code page on Windows, UTF-8 on POSIX. A folder and a file are made here
// under a name outside ASCII by the system's own wide call, and every narrow
// road to them must arrive: IsDirectory, IsFile, a stream through Path, the
// walk (whose names must print into a path the C runtime opens), GetEnv.

// A leaf outside ASCII that the narrow encoding holds: its narrow bytes in
// `narrow`, the path the system knows it by in `leaf`. False when the encoding
// holds none of the candidates (or is UTF-8 on Windows, where the code-page
// road is not the one under test).
bool NonAsciiLeaf(std::string* narrow, fs::path* leaf) {
#ifdef _WIN32
    if (GetACP() == CP_UTF8) {
        std::printf("os_tests: note: the ANSI code page is UTF-8; the code-page cases are skipped\n");
        return false;
    }
    // Western, Cyrillic, Greek, then the CJK pages (Simplified Chinese is the PC release's own).
    for (const wchar_t* w : {L"café", L"Жз", L"Ωμ", L"中文", L"あア", L"한"}) {
        char bytes[32];
        BOOL lost = FALSE;
        const int n = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, w, -1, bytes, sizeof bytes, nullptr, &lost);
        if (n <= 1 || lost) continue;
        *narrow = bytes;
        *leaf = fs::path(w);
        return true;
    }
    std::printf("os_tests: note: ANSI code page %u holds none of the test names; the code-page cases are skipped\n", GetACP());
    return false;
#else
    *narrow = "caf\xC3\xA9";
    *leaf = fs::path(*narrow);
    return true;
#endif
}

void TestNarrowNames() {
    const std::string dir = Scratch() + "_n";
    std::error_code ec;
    fs::create_directories(os::Path(dir.c_str()), ec);
    CHECK(os::IsDirectory(dir.c_str()));
    // Path and Narrow are inverses for a name the encoding holds.
    CHECK(os::Narrow(os::Path(dir.c_str())) == os::Narrow(os::Path(os::Narrow(os::Path(dir.c_str())).c_str())));
    CHECK(os::Path(nullptr).empty() && os::Path("").empty() && os::Narrow(fs::path()).empty());

    std::string leaf;
    fs::path sys_leaf;
    if (NonAsciiLeaf(&leaf, &sys_leaf)) {
        // Made by the system's own name, not through the seam.
        const fs::path sys_dir = os::Path(dir.c_str()) / sys_leaf;
        fs::create_directories(sys_dir, ec);
        CHECK(!ec);
        std::ofstream(sys_dir / sys_leaf, std::ios::binary) << "narrow";
        fs::path with_ext = sys_leaf;
        with_ext += ".DAT";
        std::ofstream(sys_dir / with_ext, std::ios::binary) << "dat";

        const std::string ndir = dir + "\\" + leaf;          // the game's own spelling
        const std::string nfile = ndir + "\\" + leaf;
        CHECK(os::Narrow(os::Path(ndir.c_str()).filename()) == leaf);
        CHECK(os::IsDirectory(ndir.c_str()) && !os::IsFile(ndir.c_str()));
        CHECK(os::IsFile(nfile.c_str()) && !os::IsDirectory(nfile.c_str()));
        std::string read;
        std::ifstream in(os::Path(nfile.c_str()), std::ios::binary);
        CHECK(static_cast<bool>(in) && static_cast<bool>(in >> read) && read == "narrow");

        // The walk: the folder by its narrow name from its parent, and its files from inside.
        bool saw = false;
        os::WalkDirectory((dir + "/*").c_str(), [&](const os::DirEntry& e) {
            if (e.name == leaf) saw = e.is_directory;
            return true;
        });
        CHECK(saw);
        std::vector<std::string> names;
        CHECK(os::WalkDirectory((ndir + "\\*.DAT").c_str(), [&](const os::DirEntry& e) {
            names.push_back(e.name);
            CHECK(e.size == 3 && !e.is_directory);
            return true;
        }) == 1);
        CHECK(names.size() == 1 && names[0] == leaf + ".DAT");
        // A pattern with the name in it.
        CHECK(os::WalkDirectory((ndir + "\\" + leaf + "*").c_str(), [](const os::DirEntry&) { return true; }) == 2);
        // What a caller does with a walked name: print it into a path for the C runtime (the game's Crt_fopen).
        if (!names.empty()) {
            char built[os::kMaxPath * 2];
            std::snprintf(built, sizeof built, "%s%c%s", os::Narrow(os::Path(ndir.c_str())).c_str(),
                          static_cast<char>(fs::path::preferred_separator), names[0].c_str());
            std::FILE* f = std::fopen(built, "rb");
            CHECK(f != nullptr);
            if (f) {
                char three[4] = {};
                CHECK(std::fread(three, 1, 3, f) == 3 && std::strcmp(three, "dat") == 0);
                std::fclose(f);
            }
            CHECK(os::IsFile(built));
        }

        // The environment: a path set by the system's own (wide) call arrives narrow and opens.
#ifdef _WIN32
        SetEnvironmentVariableW(L"BOF3X_OS_NARROW", sys_dir.c_str());
#else
        setenv("BOF3X_OS_NARROW", sys_dir.c_str(), 1);
#endif
        char env[os::kMaxPath * 2];
        const std::uint32_t n = os::GetEnv("BOF3X_OS_NARROW", env, sizeof env);
        CHECK(n > 0 && n < sizeof env);
        if (n > 0 && n < sizeof env) {
            CHECK(os::IsDirectory(env));
            CHECK(os::IsFile((std::string(env) + "\\" + leaf).c_str()));
            CHECK(os::Narrow(os::Path(env)) == os::Narrow(sys_dir));
        }
    }

    // Bytes the narrow encoding does not define (a lone lead byte in a double-byte page, 0x81 in a
    // Western one, not UTF-8 anywhere): not there, and nothing thrown.
    bool threw = false;
    try {
        const std::string bad = dir + "/bad\x81\xFE\xFF" "name";
        CHECK(!os::IsDirectory(bad.c_str()) && !os::IsFile(bad.c_str()));
        CHECK(!os::IsDirectory("\xFF") && !os::IsFile("\xC3"));
        std::ifstream in(os::Path(bad.c_str()), std::ios::binary);
        CHECK(!in);
        CHECK(os::WalkDirectory((bad + "/*").c_str(), [](const os::DirEntry&) { return true; }) == 0);
        CHECK(os::WalkDirectory((dir + "/\xFF\xFE*").c_str(), [](const os::DirEntry&) { return true; }) == 0);
        (void)os::Narrow(os::Path(bad.c_str()));
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);

    fs::remove_all(os::Path(dir.c_str()), ec);
    CHECK(!os::IsDirectory(dir.c_str()));
}

// ---- clocks -----------------------------------------------------------------

void TestClocks() {
    const std::uint64_t t0 = os::TickMs64();
    const std::uint64_t c0 = os::Counter();
    const std::uint64_t f = os::CounterFrequency();
    CHECK(f >= 1000);
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    const std::uint64_t t1 = os::TickMs64();
    const std::uint64_t c1 = os::Counter();
    CHECK(t1 >= t0 && t1 - t0 >= 30 && t1 - t0 < 5000);   // Windows' tick steps 15.6 ms
    CHECK(c1 > c0);
    const double seconds = static_cast<double>(c1 - c0) / static_cast<double>(f);
    CHECK(seconds >= 0.055 && seconds < 5.0);
    CHECK(os::TickMs32() == static_cast<std::uint32_t>(os::TickMs32()));
    const std::uint32_t a = os::TickMs32();
    CHECK(static_cast<std::uint32_t>(os::TickMs64()) - a < 1000);
    std::uint64_t last = os::Counter();
    bool monotonic = true;
    for (int i = 0; i < 100000; ++i) {
        const std::uint64_t c = os::Counter();
        monotonic = monotonic && c >= last;
        last = c;
    }
    CHECK(monotonic);
}

void TestEnvironment() {
#ifdef _WIN32
    SetEnvironmentVariableA("BOF3X_OS_TEST", "hello");
    SetEnvironmentVariableW(L"BOF3X_OS_TESTW", L"wé");
    SetEnvironmentVariableA("BOF3X_OS_EMPTY", nullptr);
#else
    setenv("BOF3X_OS_TEST", "hello", 1);
    setenv("BOF3X_OS_TESTW", "w\xC3\xA9", 1);
    unsetenv("BOF3X_OS_EMPTY");
#endif
    char buf[16];
    CHECK(os::GetEnv("BOF3X_OS_TEST", buf, sizeof buf) == 5 && std::strcmp(buf, "hello") == 0);
    CHECK(os::GetEnv("BOF3X_OS_TEST", nullptr, 0) == 6);   // too small: the size with the terminator
    char small[3];
    CHECK(os::GetEnv("BOF3X_OS_TEST", small, sizeof small) == 6);
    CHECK(os::GetEnv("BOF3X_OS_EMPTY", buf, sizeof buf) == 0);
    CHECK(os::GetEnv("BOF3X_OS_EMPTY", nullptr, 0) == 0);
    wchar_t wbuf[16];
    CHECK(os::GetEnvW(L"BOF3X_OS_TESTW", wbuf, 16) == 2 && wbuf[0] == L'w' && wbuf[1] == 0xE9);
    CHECK(os::GetEnvW(L"BOF3X_OS_EMPTY", wbuf, 16) == 0);

    CHECK(os::ToUtf8(L"café") == "caf\xC3\xA9");
    CHECK(os::ToWide("caf\xC3\xA9") == L"café");
    CHECK(os::ToUtf8(L"").empty() && os::ToWide("").empty());

    std::string exe, mod;
    CHECK(os::ExecutablePath(&exe) && !exe.empty());
    CHECK(os::ModulePath(reinterpret_cast<const void*>(&TestEnvironment), &mod) && !mod.empty());
    // UTF-8, not a narrow name (os/env.h): back to a wide path to ask the system.
    CHECK(os::IsFile(os::ToWide(exe.c_str()).c_str()) && os::IsFile(os::ToWide(mod.c_str()).c_str()));
    CHECK(os::ThreadId() != 0 && os::ProcessId() != 0);
}

// ---- hashing (src/common/sha256.cpp, the portable routine, in place of BCrypt) ------

void TestSha256() {
    std::string hex;
    CHECK(bof3x::Sha256HexBytes("", 0, hex) && hex == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(bof3x::Sha256HexBytes("abc", 3, hex) && hex == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const char* two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(bof3x::Sha256HexBytes(two, std::strlen(two), hex) && hex == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    // A million 'a' through the file reader (many blocks, an uneven tail).
    const std::string dir = Scratch();
    std::error_code ec;
    fs::create_directories(os::Path(dir.c_str()), ec);
    const std::string path = dir + "/million.bin";
    const std::vector<char> a(1000000, 'a');
    std::ofstream(os::Path(path.c_str()), std::ios::binary).write(a.data(), static_cast<std::streamsize>(a.size()));
    std::string why;
    CHECK(bof3x::Sha256Hex(os::Path(path.c_str()).wstring().c_str(), hex, why) &&
          hex == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    CHECK(!bof3x::Sha256Hex(os::Path((dir + "/none").c_str()).wstring().c_str(), hex, why) && !why.empty());
    fs::remove_all(os::Path(dir.c_str()), ec);
}

}  // namespace

int main() {
    TestMemory();
    TestFiles();
    TestNarrowNames();
    TestClocks();
    TestEnvironment();
    TestSha256();
    std::printf("os_tests: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
