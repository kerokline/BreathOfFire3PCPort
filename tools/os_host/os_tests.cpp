// Tests of the operating-system seam (src/os/os.h) on the machine they are
// built on: every service the engine uses, with synthetic data only. Built
// with the system compiler on Linux (src/os/posix) in CI, and on Windows with
// src/os/win32 where a host has one. See tools/os_host/CMakeLists.txt.
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
    return (fs::temp_directory_path() / ("bof3x_os_tests_" + std::to_string(os::ProcessId()))).string();
}

void Touch(const std::string& path) { std::ofstream(os::Path(path.c_str()), std::ios::binary) << "x"; }

void TestFiles() {
    const std::string dir = Scratch();
    std::error_code ec;
    fs::create_directories(dir, ec);
    CHECK(os::IsDirectory(dir.c_str()) && !os::IsFile(dir.c_str()));

    Touch(dir + "/a.bin");
    CHECK(os::IsFile((dir + "/a.bin").c_str()) && !os::IsDirectory((dir + "/a.bin").c_str()));
    CHECK(!os::IsFile((dir + "/missing").c_str()));
    // A backslash in the game's own spelling reads as a separator.
    CHECK(os::IsFile((dir + "\\a.bin").c_str()));
    // A non-ASCII name round-trips through UTF-8.
    const std::string uni = dir + "/caf\xC3\xA9.txt";
    Touch(uni);
    CHECK(os::IsFile(uni.c_str()));
    CHECK(os::Path("DAT\\AREA151.DAT").filename() == fs::path("AREA151.DAT"));

    // Directory walk: a pattern, name order, no dot entries, early stop.
    for (const char* name : {"AREA001.DAT", "AREA010.DAT", "area002.dat", "AREA003.EMI", "OTHER.DAT"}) Touch(dir + "/" + name);
    fs::create_directories(dir + "/AREAdir", ec);
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
    fs::remove_all(dir, ec);
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
    CHECK(os::IsFile(exe.c_str()));
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
    fs::create_directories(dir, ec);
    const std::string path = dir + "/million.bin";
    const std::vector<char> a(1000000, 'a');
    std::ofstream(os::Path(path.c_str()), std::ios::binary).write(a.data(), static_cast<std::streamsize>(a.size()));
    std::string why;
    CHECK(bof3x::Sha256Hex(os::ToWide(path.c_str()).c_str(), hex, why) &&
          hex == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    CHECK(!bof3x::Sha256Hex(os::ToWide((dir + "/none").c_str()).c_str(), hex, why) && !why.empty());
    fs::remove_all(dir, ec);
}

}  // namespace

int main() {
    TestMemory();
    TestFiles();
    TestClocks();
    TestEnvironment();
    TestSha256();
    std::printf("os_tests: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
