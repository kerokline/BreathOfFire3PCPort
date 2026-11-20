// Tests of the operating-system seam (src/os/os.h) on the machine they are
// built on: every service the engine uses, with synthetic data only. Built
// with the system compiler on Linux (src/os/posix) and with llvm-mingw on
// Windows (src/os/win32), both in CI. See tools/os_host/CMakeLists.txt.
#include "common/sha256.h"
#include "common/strto32.h"
#include "os/os.h"

#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
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
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
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

// common/strto32.h: one text, one 32-bit answer on every width. The table is
// what a 32-bit `long` library returns (held to std::strtol / std::strtoul
// themselves where `long` is 32 bits, which is every Windows build); the
// narrowing a 64-bit `long` build takes is run here on strtoll / strtoull's
// results, so a Windows run checks the Linux path's arithmetic too.
void TestStrTo32() {
    struct Signed { const char* text; int base; std::int32_t want; };
    const Signed kSigned[] = {
        {"0", 10, 0}, {"10", 10, 10}, {"-7", 10, -7}, {"  42", 10, 42}, {"2147483647", 10, INT32_MAX},
        {"2147483648", 10, INT32_MAX}, {"-2147483648", 10, INT32_MIN}, {"-2147483649", 10, INT32_MIN},
        {"4294967298", 10, INT32_MAX}, {"-4294967298", 10, INT32_MIN}, {"99999999999999", 10, INT32_MAX},
        {"99999999999999999999999", 10, INT32_MAX}, {"0x7FFFFFFF", 0, INT32_MAX}, {"0x100000002", 0, INT32_MAX},
        {"12abc", 10, 12}, {"", 10, 0}, {"x", 10, 0},
    };
    for (const Signed& c : kSigned) {
        char *end = nullptr, *wide_end = nullptr, *lib_end = nullptr;
        CHECK(bof3::StrToL32(c.text, &end, c.base) == c.want);
        CHECK(bof3::Narrow32(std::strtoll(c.text, &wide_end, c.base)) == c.want);
        CHECK(end == wide_end);
        if (sizeof(long) == 4) {
            CHECK(static_cast<std::int32_t>(std::strtol(c.text, &lib_end, c.base)) == c.want);
            CHECK(end == lib_end);
        }
    }
    struct Unsigned { const char* text; int base; std::uint32_t want; };
    const Unsigned kUnsigned[] = {
        {"0", 10, 0}, {"10", 10, 10}, {"4294967295", 10, UINT32_MAX}, {"4294967296", 10, UINT32_MAX},
        {"4294967298", 10, UINT32_MAX}, {"99999999999999", 10, UINT32_MAX}, {"99999999999999999999999", 10, UINT32_MAX},
        {"-0", 10, 0}, {"-1", 10, UINT32_MAX}, {"-2", 10, 0xFFFFFFFEu}, {" \t-2", 10, 0xFFFFFFFEu},
        {"-4294967295", 10, 1}, {"-4294967296", 10, UINT32_MAX}, {"-4294967298", 10, UINT32_MAX},
        {"-99999999999999999999999", 10, UINT32_MAX}, {"FFFFFFFF", 16, UINT32_MAX}, {"100000002", 16, UINT32_MAX},
        {"0x4A7F10", 0, 0x4A7F10}, {"12,13", 10, 12}, {"", 10, 0}, {"-", 10, 0},
    };
    for (const Unsigned& c : kUnsigned) {
        char *end = nullptr, *wide_end = nullptr, *lib_end = nullptr;
        CHECK(bof3::StrToUL32(c.text, &end, c.base) == c.want);
        errno = 0;
        const unsigned long long wide = std::strtoull(c.text, &wide_end, c.base);
        const bool overflowed = errno == ERANGE;
        CHECK(bof3::NarrowU32(wide, bof3::StrToMinus(c.text), overflowed) == c.want);
        CHECK(end == wide_end);
        if (sizeof(unsigned long) == 4) {
            CHECK(static_cast<std::uint32_t>(std::strtoul(c.text, &lib_end, c.base)) == c.want);
            CHECK(end == lib_end);
        }
    }
    std::printf("strto32: %zu signed and %zu unsigned texts give the 32-bit library's answers (long is %zu bytes here)\n",
                std::size(kSigned), std::size(kUnsigned), sizeof(long));
}

// The search with a handle (os/find.h) and the two stream questions
// (os/stream.h): what the game's save list and its file layer ask.
void TestFindAndStreams() {
    const std::string dir = Scratch() + "_find";
    std::error_code ec;
    fs::create_directories(os::Path(dir.c_str()), ec);
    for (const char* name : {"SAVE1.DAT", "save0.dat", "SAVE2.DAT", "OTHER.TXT"}) Touch(dir + "/" + name);
    std::ofstream(os::Path((dir + "/SAVE2.DAT").c_str()), std::ios::binary) << "twelve bytes";
    fs::create_directories(os::Path((dir + "/SAVEDIR.DAT").c_str()), ec);
    const std::int32_t now = static_cast<std::int32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    // The game's own spelling: a backslash before the leaf pattern.
    os::FindData found{};
    const std::int32_t h = os::FindFirst((dir + "\\SAVE*.DAT").c_str(), &found);
    CHECK(h != -1);
    std::vector<std::string> names;
    std::vector<std::uint32_t> sizes, attribs;
    bool times = true;
    if (h != -1) {
        do {
            names.push_back(found.name);
            sizes.push_back(found.size);
            attribs.push_back(found.attrib);
            times = times && found.time_write > now - 3600 && found.time_write < now + 3600;
        } while (os::FindNext(h, &found) == 0);
        CHECK(os::FindNext(h, &found) == -1);   // the end stays the end
        CHECK(os::FindClose(h) == 0);
    }
    // ASCII case counts neither in the match nor in the order; the folder that matches is handed too.
    const std::vector<std::string> want = {"save0.dat", "SAVE1.DAT", "SAVE2.DAT", "SAVEDIR.DAT"};
    CHECK(names == want);
    CHECK(sizes.size() == 4 && sizes[0] == 1 && sizes[1] == 1 && sizes[2] == 12);
    CHECK(attribs.size() == 4 && !(attribs[0] & os::kFindDirectory) && (attribs[3] & os::kFindDirectory));
    CHECK(times);
    CHECK(os::FindFirst((dir + "\\NONE*.DAT").c_str(), &found) == -1);
    // Two searches at once keep their own places.
    os::FindData a{}, b{};
    const std::int32_t ha = os::FindFirst((dir + "/SAVE?.DAT").c_str(), &a);
    const std::int32_t hb = os::FindFirst((dir + "/*.TXT").c_str(), &b);
    CHECK(ha != -1 && hb != -1 && ha != hb && std::string(a.name) == "save0.dat" && std::string(b.name) == "OTHER.TXT");
    CHECK(os::FindNext(hb, &b) == -1 && os::FindNext(ha, &a) == 0 && std::string(a.name) == "SAVE1.DAT");
    CHECK(os::FindClose(ha) == 0 && os::FindClose(hb) == 0);

    std::FILE* const f = std::fopen((dir + "/SAVE2.DAT").c_str(), "rb");
    CHECK(f != nullptr);
    if (f) {
        const int fd = os::StreamDescriptor(f);
        CHECK(fd > 2 && os::DescriptorLength(fd) == 12);
        std::fclose(f);
    }
    CHECK(os::DescriptorLength(-1) == -1);
    fs::remove_all(os::Path(dir.c_str()), ec);
}

// ---- the heap arena (os/heap.h; docs/linux-round.md "R10 as run") --------------

// A small deterministic generator, so a failing round can be replayed.
struct ArenaRng {
    std::uint64_t s;
    std::uint32_t Next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

bool Below4G(const void* p, std::size_t n) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p)) + n <= 0x100000000ull;
}

void TestHeapArena() {
    // The C runtime's exact size: _msize on Windows, none on POSIX.
    void* const m = std::malloc(37);
    std::size_t exact = 0;
#ifdef _WIN32
    CHECK(os::HeapHasExactSize() && os::HeapExactSize(m, &exact) && exact == 37);
#else
    CHECK(!os::HeapHasExactSize() && !os::HeapExactSize(m, &exact) && exact == 0);
#endif
    std::free(m);

    // A reservation below 4 GB, at or above the floor asked.
    void* const r = os::MemReserveBelow4G(0x30000, 0x80000000u);
    if (sizeof(void*) == 8) CHECK(r != nullptr);   // 32-bit: there may be nothing above 2 GB
    if (r) {
        CHECK(Below4G(r, 0x30000) && reinterpret_cast<std::uintptr_t>(r) >= 0x80000000u &&
              reinterpret_cast<std::uintptr_t>(r) % 0x10000 == 0);
        os::MemInfo mi;
        CHECK(os::MemQuery(r, &mi) && mi.state == os::MemState::Reserved);
        CHECK(os::MemRelease(r, 0x30000));
    }
    CHECK(os::MemReserveBelow4G(0, 0x10000) == nullptr);

    // Open: the whole arena below 4 GB; a second Open refused.
    constexpr std::size_t kBytes = 16u << 20;
    static os::LowArena a;   // static: never destroyed with blocks live, as the engine keeps its own
    CHECK(a.Open(kBytes));
    CHECK(!a.Open(kBytes));
    CHECK(a.Base() != 0 && Below4G(reinterpret_cast<void*>(a.Base()), a.Reserved()) && a.Reserved() == kBytes);
    CHECK(a.Committed() == 0 && a.InUse() == 0);
    const char* why = nullptr;
    CHECK(a.Verify(&why));

    // The C runtime's edges: zero-size blocks are blocks of their own, size 0;
    // a request past _HEAP_MAXREQ and a calloc product past it are refused;
    // a block bigger than the arena is refused; free of a block not live is
    // refused (the engine's Fatal).
    void* const z1 = a.Alloc(0);
    void* const z2 = a.Alloc(0);
    std::size_t sz = 99;
    CHECK(z1 && z2 && z1 != z2 && a.SizeOf(z1, &sz) && sz == 0);
    CHECK(a.Alloc(0xFFFFFFF0u) == nullptr && a.Calloc(0x10000, 0x10000) == nullptr && a.Alloc(kBytes) == nullptr);
    CHECK(a.Free(z1) && !a.Free(z1) && a.Free(z2));
    int outside = 0;
    CHECK(!a.Owns(&outside) && !a.SizeOf(&outside, &sz) && !a.Owns(nullptr));
    void* const zr = a.Realloc(nullptr, 0);   // realloc(nullptr, 0) is malloc(0)
    CHECK(zr != nullptr && a.SizeOf(zr, &sz) && sz == 0);
    CHECK(a.Verify(&why));

    // Random rounds of malloc / calloc / realloc / free against a model: each
    // live block's size, alignment, place below 4 GB and contents kept.
    struct Live {
        unsigned char* p;
        std::size_t n;
        unsigned char fill;
    };
    std::vector<Live> live;
    ArenaRng rng{0x5EED0BF3ull};
    std::size_t model_bytes = 0;
    bool aligned = true, low = true, sizes = true, kept = true, zeroed = true, invariant = true, frees = true;
    auto check_contents = [&](const Live& l) {
        for (std::size_t i = 0; i < l.n; ++i)
            if (l.p[i] != static_cast<unsigned char>(l.fill + i)) return false;
        return true;
    };
    auto fill = [](Live& l) {
        for (std::size_t i = 0; i < l.n; ++i) l.p[i] = static_cast<unsigned char>(l.fill + i);
    };
    for (unsigned round = 0; round < 20000; ++round) {
        const std::uint32_t op = rng.Next() % 10;
        // sizes mostly small, now and then up to 256 KB, sometimes 0
        const std::uint32_t pick = rng.Next();
        std::size_t n = (pick % 8 == 0) ? (rng.Next() % 0x40000) : (pick % 13 == 0) ? 0 : (rng.Next() % 600);
        if (op < 4 || live.empty()) {
            const bool zero = op == 1;
            auto* p = static_cast<unsigned char*>(zero ? a.Calloc(1, n) : a.Alloc(n));
            if (!p) continue;   // the arena full: allowed, the model is unchanged
            if (zero)
                for (std::size_t i = 0; i < n; ++i) zeroed = zeroed && p[i] == 0;
            Live l{p, n, static_cast<unsigned char>(rng.Next())};
            fill(l);
            live.push_back(l);
            model_bytes += n;
        } else if (op < 7) {
            const std::size_t i = rng.Next() % live.size();
            kept = kept && check_contents(live[i]);
            frees = frees && a.Free(live[i].p);
            model_bytes -= live[i].n;
            live[i] = live.back();
            live.pop_back();
        } else {
            const std::size_t i = rng.Next() % live.size();
            Live& l = live[i];
            if (n == 0) n = 1;   // realloc to 0 frees: tried apart below
            auto* q = static_cast<unsigned char*>(a.Realloc(l.p, n));
            if (!q) {
                kept = kept && check_contents(l);   // refused: the old block untouched
                continue;
            }
            const std::size_t keep = std::min(l.n, n);
            for (std::size_t k = 0; k < keep; ++k) kept = kept && q[k] == static_cast<unsigned char>(l.fill + k);
            model_bytes = model_bytes - l.n + n;
            l.p = q;
            l.n = n;
            fill(l);
        }
        if (!live.empty()) {
            const Live& l = live.back();
            aligned = aligned && reinterpret_cast<std::uintptr_t>(l.p) % os::LowArena::kAlign == 0;
            low = low && Below4G(l.p, l.n) && a.Owns(l.p);
            std::size_t got = 0;
            sizes = sizes && a.SizeOf(l.p, &got) && got == l.n;
        }
        if (round % 500 == 0) invariant = invariant && a.Verify(&why);
    }
    CHECK(aligned);
    CHECK(low);
    CHECK(sizes);
    CHECK(kept);
    CHECK(zeroed);
    CHECK(invariant);
    CHECK(frees);
    CHECK(a.InUse() == model_bytes);
    bool all_sizes = true;
    for (const Live& l : live) {
        std::size_t got = 0;
        all_sizes = all_sizes && a.SizeOf(l.p, &got) && got == l.n && check_contents(l);
    }
    CHECK(all_sizes);
    if (!a.Verify(&why)) std::fprintf(stderr, "arena: %s\n", why);

    // realloc to 0 frees and answers null.
    void* const g = a.Alloc(100);
    CHECK(g && a.Realloc(g, 0) == nullptr && !a.SizeOf(g, &sz));

    // Coalescing: free everything (in a scattered order), then one block of
    // nearly the arena's size must fit.
    for (std::size_t i = 0; i < live.size(); ++i) std::swap(live[i], live[rng.Next() % live.size()]);
    bool freed = true;
    for (const Live& l : live) freed = freed && a.Free(l.p);
    live.clear();
    CHECK(freed);
    CHECK(a.Free(zr));
    CHECK(a.End() == 0);   // everything back to the wilderness
    CHECK(a.Verify(&why));
    // Fill with small blocks to the end, free all, then the one big block.
    std::vector<void*> small;
    while (void* p = a.Alloc(4000)) small.push_back(p);
    CHECK(small.size() > (kBytes / 4100));
    CHECK(a.Committed() == kBytes);
    bool halves = true;
    for (std::size_t i = 0; i < small.size(); i += 2) halves = halves && a.Free(small[i]);
    CHECK(halves && a.Verify(&why));   // every other block free: no two free neighbours yet
    for (std::size_t i = 1; i < small.size(); i += 2) halves = halves && a.Free(small[i]);
    CHECK(halves && a.Verify(&why));
    std::printf("arena: %u blocks of 4000 filled %u MB, freed; ", static_cast<unsigned>(small.size()),
                static_cast<unsigned>(kBytes >> 20));
    const std::size_t in_use = a.InUse();
    CHECK(in_use == 0);
    CHECK(a.End() == 0);
    void* const big = a.Alloc(kBytes - 0x1000);
    CHECK(big != nullptr && Below4G(big, kBytes - 0x1000));
    std::printf("one block of %u bytes at %p\n", static_cast<unsigned>(kBytes - 0x1000), big);
    if (big) {
        std::memset(big, 0xA5, kBytes - 0x1000);
        CHECK(a.SizeOf(big, &sz) && sz == kBytes - 0x1000);
        // grow it in place into the wilderness, then shrink it, then free it
        CHECK(a.Realloc(big, kBytes - 0x100) == big && a.Realloc(big, 64) == big && a.SizeOf(big, &sz) && sz == 64);
        CHECK(a.Free(big));
    }
    CHECK(a.Verify(&why) && a.InUse() == 0);

    // Threads: four at once, each its own blocks.
    std::vector<std::thread> threads;
    bool threads_ok[4] = {true, true, true, true};
    for (unsigned t = 0; t < 4; ++t)
        threads.emplace_back([&threads_ok, t] {
            ArenaRng tr{t + 1ull};
            std::vector<std::pair<unsigned char*, std::size_t>> mine;
            for (unsigned i = 0; i < 4000; ++i) {
                if (mine.empty() || tr.Next() % 3 != 0) {
                    const std::size_t n = tr.Next() % 300;
                    auto* p = static_cast<unsigned char*>(a.Alloc(n));
                    if (!p) continue;
                    std::memset(p, static_cast<int>(t), n);
                    mine.emplace_back(p, n);
                } else {
                    const std::size_t k = tr.Next() % mine.size();
                    for (std::size_t j = 0; j < mine[k].second; ++j)
                        if (mine[k].first[j] != t) threads_ok[t] = false;
                    if (!a.Free(mine[k].first)) threads_ok[t] = false;
                    mine[k] = mine.back();
                    mine.pop_back();
                }
            }
            for (auto& b : mine)
                if (!a.Free(b.first)) threads_ok[t] = false;
        });
    for (auto& th : threads) th.join();
    CHECK(threads_ok[0] && threads_ok[1] && threads_ok[2] && threads_ok[3]);
    CHECK(a.Verify(&why) && a.InUse() == 0);
    if (!a.Verify(&why)) std::fprintf(stderr, "arena: %s\n", why);
}

// ---- a UTF-8 name as a path (R23: exe_image.cpp's BOF3X_EXEIMAGE files) -----

std::string AsString(const std::u8string& u) { return std::string(u.begin(), u.end()); }

void TestUtf8Paths() {
    const fs::path base = os::Path((Scratch() + "_u8").c_str());
    std::error_code ec;
    const fs::path leaf(std::u8string(u8"中文é"));   // outside every single ANSI code page at once
    fs::path file_leaf = leaf;
    file_leaf += ".bin";
    fs::create_directories(base / leaf, ec);
    CHECK(!ec);
    std::ofstream(base / leaf / file_leaf, std::ios::binary) << "utf8";

    // The engine's spelling: UTF-8 with the game's backslashes.
    const std::string dir = AsString(base.u8string()) + "\\" + AsString(leaf.u8string());
    const std::string file = dir + "\\" + AsString(file_leaf.u8string());
    const fs::path p = os::PathOfUtf8(file.c_str());
    CHECK(!p.empty() && p.filename() == file_leaf);
    {
        std::string read;
        std::ifstream in(p, std::ios::binary);   // closed at the brace: Windows removes no open file
        CHECK(static_cast<bool>(in) && static_cast<bool>(in >> read) && read == "utf8");
    }
    CHECK(fs::equivalent(os::PathOfUtf8(dir.c_str()), base / leaf, ec) && !ec);

    // Not UTF-8: a lone lead byte, a surrogate, an overlong slash, a byte past U+10FFFF; and nothing.
    for (const char* bad : {"a\xC3", "\xED\xA0\x80", "\xC0\xAF", "\xF4\x90\x80\x80", "\xFF"})
        CHECK(os::PathOfUtf8(bad).empty());
    CHECK(os::PathOfUtf8(nullptr).empty() && os::PathOfUtf8("").empty());
    CHECK(os::PathOfUtf8("\xE4\xB8\xAD") == fs::path(std::u8string(u8"中")));

    fs::remove_all(base, ec);
    CHECK(!fs::exists(base, ec));
}

// ---- the harness's calls (linux round wave 1, group R1) ----------------------
// os/module.h, os/stack.h, os/sampler.h, os/local_time.h, os/named_event.h,
// os/path_file.h: what the state hash, the WAV dump, the stall watch, the
// input queue's frozen shots and the self-tests' stack guards ask.

const char kR1Text[] = "a constant of the test's own image";

volatile std::uint32_t g_r1_spin = 0;
volatile int g_r1_sink = 0;
std::atomic<bool> g_r1_stop{false};

void R1Spin(os::SampledThread** self, os::StackRange* stack, std::atomic<bool>* ready) {
    *self = os::SampleThisThread();
    *stack = os::CurrentStack();
    ready->store(true);
    while (!g_r1_stop.load(std::memory_order_relaxed)) g_r1_spin = g_r1_spin + 1;
}

struct R1Read {
    int calls = 0;
    std::uintptr_t ip = 0, sp = 0;
    bool held = false;   // the spin did not move while the thread was held
};

void R1OnSample(void* ctx, std::uintptr_t ip, std::uintptr_t sp) {
    auto* r = static_cast<R1Read*>(ctx);
    ++r->calls;
    r->ip = ip;
    r->sp = sp;
    const std::uint32_t a = g_r1_spin;
    for (int i = 0; i < 200000; ++i) g_r1_sink = i;   // a while, with the thread held
    r->held = g_r1_spin == a;
}

void TestR1Harness() {
    // The module that holds a function of ours: its code and its constants.
    os::ModuleImageInfo image;
    const auto code = reinterpret_cast<std::uintptr_t>(&TestR1Harness);
    const auto text = reinterpret_cast<std::uintptr_t>(kR1Text);
    CHECK(os::ModuleImage(reinterpret_cast<const void*>(&TestR1Harness), &image));
    CHECK(image.extent > 0 && code >= image.base && code < image.base + image.extent && !image.parts.empty());
    bool code_part = false, text_part = false;
    for (const os::ModulePart& p : image.parts) {
        code_part = code_part || (code >= p.lo && code < p.hi && p.executable);
        text_part = text_part || (text >= p.lo && text < p.hi && p.readable && !p.executable);
        CHECK(p.lo >= image.base && p.hi <= image.base + image.extent && p.lo < p.hi);
    }
    CHECK(code_part && text_part);
    // The base itself names the same module (a Windows module handle is its base).
    os::ModuleImageInfo again;
    CHECK(os::ModuleImage(reinterpret_cast<const void*>(image.base), &again) && again.base == image.base &&
          again.extent == image.extent && again.parts.size() == image.parts.size());
    void* const heap = os::MemAlloc(nullptr, kPage, os::Prot::ReadWrite);
    CHECK(heap && !os::ModuleImage(heap, &again));
    if (heap) os::MemRelease(heap, kPage);

    CHECK(os::ModuleExportByOrdinal("bof3x_no_such_module.dll", 1) == nullptr);
#ifdef _WIN32
    // Some ordinal of kernel32 answers, and some lies in kernel32's own image
    // (many are forwarded: GetProcAddress follows them into another DLL).
    const auto k32_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("kernel32.dll"));
    bool any = false, own = false, same = true;
    for (unsigned o = 1; o < 4096 && !own; ++o) {
        void* const p = os::ModuleExportByOrdinal("kernel32.dll", o);
        same = same && p == reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("kernel32.dll"), MAKEINTRESOURCEA(o)));
        if (!p) continue;
        any = true;
        os::ModuleImageInfo k32;
        own = os::ModuleImage(p, &k32) && k32.base == k32_base;
    }
    CHECK(any && own && same);
    CHECK(os::ModuleExportByOrdinal("kernel32.dll", 0xFFFF) == nullptr);
#else
    CHECK(os::ModuleExportByOrdinal("libc.so.6", 1) == nullptr);   // ELF numbers no export
#endif

    // The stack: a local of ours lies in it, and another thread's is another.
    int local = 0;
    const os::StackRange here = os::CurrentStack();
    const auto at = reinterpret_cast<std::uintptr_t>(&local);
    CHECK(here.limit && here.base && reinterpret_cast<std::uintptr_t>(here.limit) <= at && at < reinterpret_cast<std::uintptr_t>(here.base));
    os::StackRange there;
    std::uintptr_t there_local = 0;
    std::thread([&] {
        int other = 0;
        there = os::CurrentStack();
        there_local = reinterpret_cast<std::uintptr_t>(&other);
    }).join();
    CHECK(there.base != here.base && reinterpret_cast<std::uintptr_t>(there.limit) <= there_local &&
          there_local < reinterpret_cast<std::uintptr_t>(there.base));

    // The sampler: Windows holds a spinning thread and reads it; POSIX has none.
    if (os::CanSampleThreads()) {
        os::SampledThread* t = nullptr;
        os::StackRange spin_stack;
        std::atomic<bool> ready{false};
        g_r1_stop = false;
        std::thread spinner(&R1Spin, &t, &spin_stack, &ready);
        while (!ready.load()) std::this_thread::yield();
        R1Read r;
        CHECK(t != nullptr && os::SampleThread(t, &R1OnSample, &r));
        g_r1_stop = true;
        spinner.join();
        CHECK(r.calls == 1 && r.held);
        CHECK(r.sp >= reinterpret_cast<std::uintptr_t>(spin_stack.limit) && r.sp < reinterpret_cast<std::uintptr_t>(spin_stack.base));
        os::ModuleImageInfo at_ip;
        CHECK(os::ModuleImage(reinterpret_cast<const void*>(r.ip), &at_ip) && at_ip.base == image.base);
    } else {
        CHECK(os::SampleThisThread() == nullptr);
        R1Read r;
        CHECK(!os::SampleThread(nullptr, &R1OnSample, &r) && r.calls == 0);
    }

    // The local time of day, against the C library's.
    const std::time_t before = std::time(nullptr);
    const os::LocalTime lt = os::LocalNow();
    const std::tm* tm = std::localtime(&before);
    CHECK(lt.hour < 24 && lt.minute < 60 && lt.second < 61 && lt.millisecond < 1000);
    const int ours = static_cast<int>(lt.hour * 60 + lt.minute), theirs = tm->tm_hour * 60 + tm->tm_min;
    CHECK(ours == theirs || (ours - theirs + 1440) % 1440 == 1);

    // The named event, set by name as a driver sets it.
    const std::string name = "bof3x_os_test_" + std::to_string(os::ProcessId());
    os::NamedEvent* const e = os::CreateNamedEvent(name.c_str());
    CHECK(e != nullptr);
    if (e) {
#ifdef _WIN32
        HANDLE other = OpenEventW(EVENT_MODIFY_STATE, FALSE, os::ToWide(("Local\\" + name).c_str()).c_str());
        const auto set = [&] { SetEvent(other); };
#else
        sem_t* other = sem_open(("/" + name).c_str(), 0);
        const auto set = [&] { sem_post(other); };
#endif
        CHECK(other != nullptr);
        const auto t0 = std::chrono::steady_clock::now();
        CHECK(!os::WaitNamedEvent(e, 60));
        CHECK(std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(40));
        set();
        CHECK(os::WaitNamedEvent(e, 1000));
        CHECK(!os::WaitNamedEvent(e, 20));   // taken by the wait: an auto-reset event
        set();
        set();
        os::ResetNamedEvent(e);
        CHECK(!os::WaitNamedEvent(e, 20));
        std::thread late([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            set();
        });
        CHECK(os::WaitNamedEvent(e, 5000));
        late.join();
        std::printf("named event: the wait is %s\n", os::NamedEventWaitWay());
#ifdef _WIN32
        CloseHandle(other);
#else
        // A signal taken during the wait does not end it: the wait goes on to
        // its own bound (EINTR), and a set after the signal is still seen.
        struct sigaction act {}, old {};
        act.sa_handler = [](int) {};   // no SA_RESTART: the wait is interrupted
        sigemptyset(&act.sa_mask);
        CHECK(sigaction(SIGUSR1, &act, &old) == 0);
        const pthread_t self = pthread_self();
        {
            std::thread poke([&] {
                for (int i = 0; i < 5; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    pthread_kill(self, SIGUSR1);
                }
            });
            const auto s0 = std::chrono::steady_clock::now();
            CHECK(!os::WaitNamedEvent(e, 300));
            const auto took = std::chrono::steady_clock::now() - s0;
            CHECK(took >= std::chrono::milliseconds(290) && took < std::chrono::milliseconds(2000));
            poke.join();
        }
        {
            std::thread poke([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                pthread_kill(self, SIGUSR1);
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                set();
            });
            CHECK(os::WaitNamedEvent(e, 5000));
            poke.join();
        }
        sigaction(SIGUSR1, &old, nullptr);
        sem_close(other);
        sem_unlink(("/" + name).c_str());
#endif
    }

    // A stream on a wide path, made only when it is not there ("x").
    const std::string dir = Scratch() + "_r1";
    std::error_code ec;
    fs::create_directories(os::Path(dir.c_str()), ec);
    const fs::path file = os::Path(dir.c_str()) / fs::path(L"wav_é中.bin");
    std::FILE* f = os::OpenPath(file, "wbx");
    CHECK(f != nullptr);
    if (f) {
        CHECK(std::fwrite("RIFF....", 1, 8, f) == 8 && std::fseek(f, 4, SEEK_SET) == 0 && std::fwrite("WAVE", 1, 4, f) == 4 &&
              std::fseek(f, 0, SEEK_END) == 0 && std::fwrite("!", 1, 1, f) == 1);
        std::fclose(f);
    }
    CHECK(os::OpenPath(file, "wbx") == nullptr);   // there now: CREATE_NEW's refusal
    f = os::OpenPath(file, "rb");
    char back[16] = {};
    CHECK(f && std::fread(back, 1, sizeof back, f) == 9 && std::memcmp(back, "RIFFWAVE!", 9) == 0);
    if (f) std::fclose(f);
    fs::remove_all(os::Path(dir.c_str()), ec);
}

}  // namespace
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <atomic>
#include <iterator>
namespace {

// EndProcess is tried in a child: this program again with the variable set
// (Windows) or a fork (POSIX), which ends here with 37 before main runs.
const bool g_end_process_child = [] {
    if (os::GetEnv("BOF3X_OS_TESTS_END_PROCESS", nullptr, 0) > 0) {
        std::atexit([] { std::_Exit(99); });   // must not run
        os::EndProcess(37);
    }
    return false;
}();

std::uint32_t ChildExitCode() {
#ifdef _WIN32
    std::string self;
    if (!os::ExecutablePath(&self)) return 0xFFFFFFFFu;
    std::wstring cmd = L"\"" + os::ToWide(self.c_str()) + L"\"";
    SetEnvironmentVariableW(L"BOF3X_OS_TESTS_END_PROCESS", L"1");
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    SetEnvironmentVariableW(L"BOF3X_OS_TESTS_END_PROCESS", nullptr);
    if (!ok) return 0xFFFFFFFFu;
    WaitForSingleObject(pi.hProcess, 10000);
    DWORD code = 0xFFFFFFFFu;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
#else
    std::fflush(nullptr);
    const pid_t pid = fork();
    if (pid == 0) {
        std::atexit([] { std::_Exit(99); });
        os::EndProcess(37);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) != pid || !WIFEXITED(status)) return 0xFFFFFFFFu;
    return static_cast<std::uint32_t>(WEXITSTATUS(status));
#endif
}

struct Waiter {
    os::Event on;
    os::Event done;
    std::atomic<int>* count;
};
void WaitThenCount(void* p) {
    Waiter* w = static_cast<Waiter*>(p);
    if (os::EventWait(w->on, os::kWaitForever) == os::WaitResult::Signalled) {
        if (w->count) w->count->fetch_add(1);
        if (w->done) os::EventSet(w->done);
    }
}

bool Within(const std::function<bool()>& ok, int ms) {
    for (int i = 0; i < ms; ++i) {
        if (ok()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return ok();
}

void TestEventsThreadsFiles() {
    using os::WaitResult;
    CHECK(!g_end_process_child);
    // Auto-reset: one wait, then clear; a set kept until a wait; two sets are one.
    os::Event a = os::EventCreate(false, false);
    CHECK(a != nullptr);
    CHECK(os::EventWait(a, 0) == WaitResult::Timeout);
    CHECK(os::EventSet(a) && os::EventSet(a));
    CHECK(os::EventWait(a, 0) == WaitResult::Signalled);
    CHECK(os::EventWait(a, 0) == WaitResult::Timeout);
    os::Event s = os::EventCreate(false, true);
    CHECK(os::EventWait(s, 0) == WaitResult::Signalled && os::EventWait(s, 0) == WaitResult::Timeout);
    // Manual-reset: stays set for every wait until reset.
    os::Event m = os::EventCreate(true, false);
    CHECK(os::EventSet(m) && os::EventWait(m, 0) == WaitResult::Signalled && os::EventWait(m, 0) == WaitResult::Signalled);
    CHECK(os::EventReset(m) && os::EventWait(m, 0) == WaitResult::Timeout);
    // A timed wait waits.
    const auto t0 = std::chrono::steady_clock::now();
    CHECK(os::EventWait(a, 60) == WaitResult::Timeout);
    CHECK(std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(40));
    // Across threads: an auto-reset set releases one of two waiters, a manual-reset set both.
    std::atomic<int> count{0};
    Waiter w1{a, nullptr, &count}, w2{a, nullptr, &count};
    os::Thread t1 = os::StartThread(WaitThenCount, &w1), t2 = os::StartThread(WaitThenCount, &w2);
    CHECK(t1 && t2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(count.load() == 0 && !os::ThreadEnded(t1) && !os::ThreadEnded(t2));
    CHECK(os::EventSet(a));
    CHECK(Within([&] { return count.load() == 1; }, 2000));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(count.load() == 1 && os::ThreadEnded(t1) != os::ThreadEnded(t2));
    CHECK(os::EventSet(a));
    CHECK(Within([&] { return count.load() == 2 && os::ThreadEnded(t1) && os::ThreadEnded(t2); }, 2000));
    // Two waiters blocked, then two sets with nothing between them: each set
    // releases one waiter at the set itself, so both end and nothing is left
    // set. (Windows hands a set to a blocked waiter in the kernel; a backend
    // that only raises a flag and wakes a thread loses the second set when it
    // comes before the first waiter has run.)
    int lost = 0, left = 0;
    for (int round = 0; round < 10; ++round) {
        std::atomic<int> pair{0};
        Waiter p1{a, nullptr, &pair}, p2{a, nullptr, &pair};
        os::Thread u1 = os::StartThread(WaitThenCount, &p1), u2 = os::StartThread(WaitThenCount, &p2);
        if (!u1 || !u2) {
            ++lost;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        os::EventSet(a);
        os::EventSet(a);
        const auto ended = [&] { return pair.load() == 2 && os::ThreadEnded(u1) && os::ThreadEnded(u2); };
        if (!Within(ended, 1000)) {
            ++lost;
            // The waiter left behind is let go: it holds this round's stack.
            for (int i = 0; i < 50 && !ended(); ++i) {
                os::EventSet(a);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }
        if (os::EventWait(a, 0) == WaitResult::Signalled) ++left;
    }
    CHECK(lost == 0);
    CHECK(left == 0);
    // A waiter that gave up is not handed a later set: the set stays for the next wait.
    CHECK(os::EventWait(a, 30) == WaitResult::Timeout);
    CHECK(os::EventSet(a) && os::EventWait(a, 0) == WaitResult::Signalled && os::EventWait(a, 0) == WaitResult::Timeout);
    std::atomic<int> both{0};
    Waiter w3{m, nullptr, &both}, w4{m, nullptr, &both};
    CHECK(os::StartThread(WaitThenCount, &w3) && os::StartThread(WaitThenCount, &w4));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(os::EventSet(m));
    CHECK(Within([&] { return both.load() == 2; }, 2000));
#ifndef _WIN32
    // A manual-reset set releases the waits blocked at the set, whatever comes
    // after: a reset straight behind it takes nothing back. (Not asked of
    // Windows, where it is PulseEvent's case: a wait the kernel has borrowed
    // for a moment is not there to be released.)
    CHECK(os::EventReset(m));
    std::atomic<int> pulse{0};
    Waiter w6{m, nullptr, &pulse}, w7{m, nullptr, &pulse};
    os::Thread t6 = os::StartThread(WaitThenCount, &w6), t7 = os::StartThread(WaitThenCount, &w7);
    CHECK(t6 && t7);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(os::EventSet(m) && os::EventReset(m));
    CHECK(Within([&] { return pulse.load() == 2 && os::ThreadEnded(t6) && os::ThreadEnded(t7); }, 2000));
    CHECK(os::EventWait(m, 0) == WaitResult::Timeout);
    if (pulse.load() != 2) os::EventSet(m);   // the waiters hold this function's stack
#endif
    // A wait forever, ended by a set from here; the other side's set seen here.
    os::Event done = os::EventCreate(false, false);
    Waiter w5{s, done, nullptr};
    CHECK(os::StartThread(WaitThenCount, &w5) != nullptr);
    CHECK(os::EventSet(s) && os::EventWait(done, 5000) == WaitResult::Signalled);
    // No event: refused.
    CHECK(!os::EventSet(nullptr) && os::EventWait(nullptr, 0) == WaitResult::Failed);
    for (os::Event e : {a, s, m, done}) os::EventClose(e);
    CHECK(!os::ThreadEnded(nullptr));

    // Unbuffered files: a write is readable at once, a second create empties the file.
    const std::string dir = Scratch() + "_out";
    std::error_code ec;
    fs::create_directories(os::Path(dir.c_str()), ec);
    const fs::path p = os::Path((dir + "/log.txt").c_str());
    os::OutFile f = os::OutCreate(p);
    CHECK(f != os::kNoFile);
    CHECK(os::OutWrite(f, "abc", 3));
    {
        std::ifstream in(p, std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(in.is_open() && got == "abc");   // others may read it while it is open
    }
    CHECK(os::OutWrite(f, "def\r\n", 5) && os::OutFlush(f));
    os::OutClose(f);
    CHECK(fs::file_size(p, ec) == 8);
    f = os::OutCreate(p);
    CHECK(f != os::kNoFile && fs::file_size(p, ec) == 0);
    os::OutClose(f);
    CHECK(os::OutCreate(os::Path((dir + "/no/such/dir/x").c_str())) == os::kNoFile);
    CHECK(!os::OutWrite(os::kNoFile, "x", 1) && !os::OutFlush(os::kNoFile));
    fs::remove_all(os::Path(dir.c_str()), ec);

    // The process's end: the code given, no exit handler run.
    CHECK(ChildExitCode() == 37);
}

// ---- a function on a stack of its own, and the refusing rename (wave 2, group R7) ----
// os/own_stack.h: call and return, the range os::CurrentStack() answers on it,
// nesting, the x87 control word both ways, a recursion the game's 16 KB task
// slice could not hold, a run from another thread, and (POSIX) a guard-page
// fault taken on an alternate signal stack as the crash catcher arms one.
// os/file_move.h: the rename that keeps its target and the one that replaces
// it, in the temporary folder and in the current one (the WSL's /tmp and
// /mnt/c are different filesystems), and the standard library's neutral
// answers that snapshot.cpp reads in place of Windows' error numbers.

}  // namespace

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#endif

namespace {

constexpr std::size_t kR7Reserve = 1024 * 1024;
os::OwnStack g_r7_a{{256 * 1024, kR7Reserve, false, true}};
os::OwnStack g_r7_b{{64 * 1024, 256 * 1024, false, true}};

bool R7Inside(const os::StackRange& r, const void* p) {
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    return r.limit && r.base && reinterpret_cast<std::uintptr_t>(r.limit) <= a && a < reinterpret_cast<std::uintptr_t>(r.base);
}

// 160 frames of a little over 4 KB: some 650 KB, which fits in 1 MB and not in
// the game's 16 KB slice. Each frame's local must lie on the stack the run
// reports, and the deepest one far below the top.
[[gnu::noinline]] unsigned R7Deep(unsigned n, std::uintptr_t* lowest) {
    volatile unsigned char pad[4096];
    for (unsigned i = 0; i < sizeof pad; i += 512) pad[i] = static_cast<unsigned char>(n + i);
    const auto here = reinterpret_cast<std::uintptr_t>(&pad[0]);
    if (here < *lowest) *lowest = here;
    const unsigned below = n ? R7Deep(n - 1, lowest) : 0;
    return below + pad[512];
}

struct R7Probe {
    os::StackRange seen;
    bool local_inside = false;
    std::uintptr_t lowest = ~std::uintptr_t{0};
    unsigned sum = 0;
    os::OwnStackResult nested_self = os::OwnStackResult::Done;
    os::OwnStackResult inner = os::OwnStackResult::NoStack;
    os::StackRange after_deep;   // Windows' limit is the lowest committed page: it moves down as the stack grows
    os::StackRange inner_seen, after_inner;
    bool inner_local_inside = false;
    int runs = 0;
};

void R7Inner(void* p) {
    auto* probe = static_cast<R7Probe*>(p);
    int local = 0;
    probe->inner_seen = os::CurrentStack();
    probe->inner_local_inside = R7Inside(probe->inner_seen, &local);
}

void R7Outer(void* p) {
    auto* probe = static_cast<R7Probe*>(p);
    int local = 0;
    ++probe->runs;
    probe->seen = os::CurrentStack();
    probe->local_inside = R7Inside(probe->seen, &local);
    probe->sum = R7Deep(160, &probe->lowest);
    probe->after_deep = os::CurrentStack();
    probe->nested_self = g_r7_a.Run(&R7Inner, probe);   // a run of the running stack: refused
    probe->inner = g_r7_b.Run(&R7Inner, probe);         // another stack inside this one: returns here
    probe->after_inner = os::CurrentStack();
}

unsigned short R7GetCw() {
    unsigned short cw = 0;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void R7SetCw(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
void R7ReadCw(void* p) { *static_cast<unsigned short*>(p) = R7GetCw(); }
void R7WriteCw(void* p) { R7SetCw(*static_cast<unsigned short*>(p)); }

#ifndef _WIN32
// The guard page, in a child process: an alternate signal stack and a
// SIGSEGV handler armed as hook/crash_posix.cpp arms them (sigaltstack,
// SA_SIGINFO | SA_ONSTACK), then a recursion with no end on a small own
// stack. The handler answers by its exit code: 42 when the fault's address
// is the page under the own stack and the handler runs on the alternate stack.
std::uintptr_t g_r7_guard_lo = 0, g_r7_guard_hi = 0;
alignas(16) unsigned char g_r7_alt[64 * 1024];

void R7OnSegv(int, siginfo_t* si, void*) {
    unsigned char here = 0;
    const auto at = reinterpret_cast<std::uintptr_t>(si->si_addr);
    const auto sp = reinterpret_cast<std::uintptr_t>(&here);
    const bool on_alt = sp >= reinterpret_cast<std::uintptr_t>(g_r7_alt) && sp < reinterpret_cast<std::uintptr_t>(g_r7_alt) + sizeof g_r7_alt;
    const bool in_guard = at >= g_r7_guard_lo && at < g_r7_guard_hi;
    _exit(on_alt && in_guard ? 42 : on_alt ? 43 : 44);
}

[[gnu::noinline]] unsigned R7Forever(unsigned n) {
    volatile unsigned char pad[1024];
    pad[0] = static_cast<unsigned char>(n);
    if (n == 0xFFFFFFFFu) return pad[0];   // never reached: 64 KB end long before
    return R7Forever(n + 1) + pad[0];
}

void R7Overflow(void*) {
    const os::StackRange r = os::CurrentStack();
    const auto page = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
    g_r7_guard_hi = reinterpret_cast<std::uintptr_t>(r.limit);
    g_r7_guard_lo = g_r7_guard_hi - page;
    R7Forever(0);
}

int R7GuardChild() {
    const pid_t child = fork();
    if (child == 0) {
        stack_t alt{};
        alt.ss_sp = g_r7_alt;
        alt.ss_size = sizeof g_r7_alt;
        struct sigaction sa{};
        sa.sa_sigaction = &R7OnSegv;
        sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&sa.sa_mask);
        if (sigaltstack(&alt, nullptr) != 0 || sigaction(SIGSEGV, &sa, nullptr) != 0) _exit(45);
        static os::OwnStack small{{0, 64 * 1024, false, true}};
        small.Run(&R7Overflow, nullptr);
        _exit(46);   // the recursion returned: it cannot
    }
    int status = 0;
    if (child < 0 || waitpid(child, &status, 0) != child) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1000 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
}
#endif

void TestOwnStack() {
    // A run: the function ran on a range that is not the thread's, holding its
    // locals; the deep recursion fit; the stack itself refused a run of itself;
    // another stack ran inside and came back to this one.
    const os::StackRange thread_stack = os::CurrentStack();
    int outer_local = 0;
    CHECK(R7Inside(thread_stack, &outer_local));
    CHECK(!g_r7_a.Made());
    R7Probe probe;
    CHECK(g_r7_a.Run(&R7Outer, &probe) == os::OwnStackResult::Done);
    CHECK(g_r7_a.Made() && probe.runs == 1);
    CHECK(probe.local_inside && probe.seen.base != thread_stack.base);
    CHECK(probe.after_deep.base == probe.seen.base && R7Inside(probe.after_deep, reinterpret_cast<void*>(probe.lowest)));
    CHECK(reinterpret_cast<std::uintptr_t>(probe.seen.base) - probe.lowest > 600 * 1024);
    CHECK(reinterpret_cast<std::uintptr_t>(probe.after_deep.base) - reinterpret_cast<std::uintptr_t>(probe.after_deep.limit) <= kR7Reserve);
    CHECK(probe.nested_self == os::OwnStackResult::Nested);
    CHECK(probe.inner == os::OwnStackResult::Done && probe.inner_local_inside);
    CHECK(probe.inner_seen.base != probe.seen.base && probe.inner_seen.base != thread_stack.base);
    CHECK(probe.after_inner.base == probe.seen.base);
    // Back on the thread's stack, the thread's range again.
    const os::StackRange again = os::CurrentStack();
    CHECK(again.base == thread_stack.base && R7Inside(again, &outer_local));
    std::printf("own stack: %p..%p (the thread's %p..%p), %u KB deep\n", probe.after_deep.limit, probe.after_deep.base, thread_stack.limit,
                thread_stack.base, static_cast<unsigned>((reinterpret_cast<std::uintptr_t>(probe.seen.base) - probe.lowest) / 1024));

    // The stack is kept: a thousand runs, the same range each time.
    bool same = true;
    for (int i = 0; i < 1000 && same; ++i) {
        os::StackRange seen;
        same = g_r7_b.Run([](void* q) { *static_cast<os::StackRange*>(q) = os::CurrentStack(); }, &seen) == os::OwnStackResult::Done &&
               seen.base == probe.inner_seen.base;
    }
    CHECK(same);
    CHECK(g_r7_a.Run(&R7Outer, &probe) == os::OwnStackResult::Done && probe.runs == 2 && probe.seen.base != thread_stack.base);

    // From another thread: the same stack, the range on that thread.
    os::StackRange from_thread;
    os::OwnStackResult thread_result = os::OwnStackResult::NoStack;
    std::thread([&] {
        thread_result = g_r7_b.Run([](void* q) { *static_cast<os::StackRange*>(q) = os::CurrentStack(); }, &from_thread);
    }).join();
    CHECK(thread_result == os::OwnStackResult::Done && from_thread.base == probe.inner_seen.base);

    // The x87 control word. Carried: the run sees the caller's word at every
    // entry. Not carried: the stack keeps the word it last had, whatever the
    // caller's is now (the fiber's float switch, swapcontext's saved state).
    const unsigned short cw0 = R7GetCw();
    static os::OwnStack carried{{64 * 1024, 256 * 1024, true, true}};
    static os::OwnStack kept{{64 * 1024, 256 * 1024, false, true}};
    unsigned short seen_cw = 0, want = 0x027F;   // the default with 53-bit precision
    R7SetCw(want);
    CHECK(carried.Run(&R7ReadCw, &seen_cw) == os::OwnStackResult::Done && seen_cw == 0x027F);
    R7SetCw(0x037F);
    CHECK(carried.Run(&R7ReadCw, &seen_cw) == os::OwnStackResult::Done && seen_cw == 0x037F);
    unsigned short inside = 0x007F;   // 24-bit precision, set on the stack
    CHECK(kept.Run(&R7WriteCw, &inside) == os::OwnStackResult::Done);
    CHECK(R7GetCw() == 0x037F);   // the caller's own word is back after the run
    CHECK(kept.Run(&R7ReadCw, &seen_cw) == os::OwnStackResult::Done && seen_cw == 0x007F);
    R7SetCw(cw0);
    std::printf("own stack: x87 control word carried 0x027F and 0x037F, kept 0x%04X\n", seen_cw);

#ifndef _WIN32
    const int guard = R7GuardChild();
    CHECK(guard == 42);
    std::printf("own stack: a recursion past the stack's end faulted on its guard page, taken on the alternate stack (child exit %d)\n",
                guard);
#endif
}

void R7Write(const fs::path& p, const char* text) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << text;
}
std::string R7Read(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

const char* R7Way(int way) {
    return way == 0 ? "renameat2 / MoveFileExW" : way == 1 ? "link and unlink" : way == 2 ? "exclusive create and rename" : "nothing";
}

void R7Moves(const fs::path& dir, const char* where) {
    std::error_code ec;
    fs::remove_all(dir, ec);
    CHECK(fs::create_directory(dir, ec) && !ec);
    const fs::path a = dir / "BISLPS1A.DAT", b = dir / "BISLPS1B.DAT", c = dir / "BISLPS1C.DAT";
    R7Write(a, "a");
    R7Write(b, "b");
    // Kept: a target that is there refuses, and nothing moves.
    CHECK(os::MoveFileKeep(a, b) == os::MoveResult::TargetExists);
    const int refused_way = os::MoveFileKeepWay();
    CHECK(R7Read(a) == "a" && R7Read(b) == "b");
    // Kept: a free target, the file moved.
    CHECK(os::MoveFileKeep(a, c) == os::MoveResult::Moved);
    const int moved_way = os::MoveFileKeepWay();
    CHECK(!fs::exists(a, ec) && R7Read(c) == "a");
    CHECK(os::MoveFileKeep(a, c) == os::MoveResult::NoSource);
    // Replaced: the target's bytes are the source's.
    CHECK(os::MoveFileReplace(c, b) == os::MoveResult::Moved);
    CHECK(!fs::exists(c, ec) && R7Read(b) == "a");
    CHECK(os::MoveFileReplace(c, b) == os::MoveResult::NoSource);
    // The neutral answers: a missing file is "no such file", a folder made twice is no error.
    (void)fs::file_size(a, ec);
    CHECK(ec == std::errc::no_such_file_or_directory);
    ec.clear();
    CHECK(!fs::create_directory(dir, ec) && !ec);
    fs::remove_all(dir, ec);
    std::printf("file move in %s (%s): the refusing rename by %s (refused) and %s (moved)\n", where, os::Narrow(dir).c_str(),
                R7Way(refused_way), R7Way(moved_way));
}

// Each way of the refusing rename, run on a system that has them all: the
// POSIX backend starts at the way asked for, as if the system had refused the
// ones before it (os/file_move.h). Windows has one way, and the same answers.
void R7MoveWays(const fs::path& dir) {
    std::error_code ec;
    for (int way = 0; way <= 2; ++way) {
#ifdef _WIN32
        const int ran = 0;
#else
        const int ran = way;
#endif
        fs::remove_all(dir, ec);
        CHECK(fs::create_directory(dir, ec) && !ec);
        const fs::path a = dir / "BISLPS1A.DAT", b = dir / "BISLPS1B.DAT", c = dir / "BISLPS1C.DAT", d = dir / "BISLPS1D.DAT";
        R7Write(a, "a");
        R7Write(b, "b");
        os::MoveFileKeepStartAt(way);
        // A target that is there refuses, and both files are as they were.
        CHECK(os::MoveFileKeep(a, b) == os::MoveResult::TargetExists && os::MoveFileKeepWay() == ran);
        CHECK(R7Read(a) == "a" && R7Read(b) == "b");
        // An empty file at the target is a file: refused too (the last way's own claim looks like one).
        R7Write(d, "");
        CHECK(os::MoveFileKeep(a, d) == os::MoveResult::TargetExists && fs::file_size(d, ec) == 0 && R7Read(a) == "a");
        fs::remove(d, ec);
        // A free target: the file moved, bytes and all.
        CHECK(os::MoveFileKeep(a, c) == os::MoveResult::Moved && os::MoveFileKeepWay() == ran);
        CHECK(!fs::exists(a, ec) && R7Read(c) == "a");
        // No source: refused as such, the file at the target kept, and no file left at a free target.
        CHECK(os::MoveFileKeep(a, c) == os::MoveResult::NoSource && R7Read(c) == "a");
        CHECK(os::MoveFileKeep(a, d) == os::MoveResult::NoSource && !fs::exists(d, ec));
        // The rotation's own shape: 1B -> 1C refused while 1C is there, then 1C away, then moved.
        CHECK(os::MoveFileKeep(b, c) == os::MoveResult::TargetExists);
        CHECK(fs::remove(c, ec) && os::MoveFileKeep(b, c) == os::MoveResult::Moved && R7Read(c) == "b" && !fs::exists(b, ec));
        int files = 0;
        for (const auto& entry : fs::directory_iterator(dir, ec)) files += entry.is_regular_file() ? 1 : 0;
        CHECK(files == 1);
        os::MoveFileKeepStartAt(0);
    }
    fs::remove_all(dir, ec);
}

void TestFileMove() {
    std::error_code ec;
    const std::string tag = "bof3x_r7_move_" + std::to_string(os::ProcessId());
    R7Moves(fs::temp_directory_path(ec) / tag, "the temporary folder");
    R7Moves(fs::current_path(ec) / tag, "the current folder");
    R7MoveWays(fs::temp_directory_path(ec) / (tag + "_ways"));
}

// ---- a key posted to a window (wave 2, I1: os/window_key.h) ----------------------
// Windows: the two messages the input recipe's `key` step posts, read back off
// a message-only window's queue with their wParam and lParam; a handle that is
// no window refuses. POSIX: the call refuses, ENOSYS.
}  // namespace
#include <cerrno>
namespace {
void TestWindowKey() {
#ifdef _WIN32
    HWND w = CreateWindowExA(0, "STATIC", "bof3x_i1", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleA(nullptr), nullptr);
    CHECK(w != nullptr);
    if (w) {
        CHECK(os::PostWindowKey(w, 0x78, true));    // F9 down
        CHECK(os::PostWindowKey(w, 0x78, false));   // F9 up
        MSG m = {};
        CHECK(PeekMessageA(&m, w, 0, 0, PM_REMOVE) != 0);
        CHECK(m.message == WM_KEYDOWN && m.wParam == 0x78 && m.lParam == 1);
        CHECK(PeekMessageA(&m, w, 0, 0, PM_REMOVE) != 0);
        CHECK(m.message == WM_KEYUP && m.wParam == 0x78 && static_cast<std::uint32_t>(m.lParam) == 0xC0000001u);
        CHECK(PeekMessageA(&m, w, 0, 0, PM_REMOVE) == 0);
        DestroyWindow(w);
    }
    CHECK(!os::PostWindowKey(reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x5A5A0)), 0x78, true));
    CHECK(os::LastError() == ERROR_INVALID_WINDOW_HANDLE);
    std::printf("window key: WM_KEYDOWN and WM_KEYUP posted and read back\n");
#else
    CHECK(!os::PostWindowKey(nullptr, 0x78, true));
    CHECK(os::LastError() == ENOSYS);
    std::printf("window key: refused (no window messages on this system)\n");
#endif
}

// ---- the case check (LB8: os/case_check.h) ----------------------------------------
// A folder and a file under mixed case, asked for under another case through
// each road the check sits on. On Windows (case-insensitive) the names open
// and the check names the disk's spelling; on a case-sensitive POSIX
// filesystem they do not open and the parent walk still names it - the Linux
// failure the check exists to find. Lines come once each, are kept until a
// sink is set, and the system's last error is left as it was.
std::vector<std::string> g_case_lines;
void CaseSinkForTest(const char* line) { g_case_lines.push_back(line); }

std::size_t CaseLinesWith(const std::string& what) {
    std::size_t n = 0;
    for (const std::string& l : g_case_lines) n += l.find(what) != std::string::npos;
    return n;
}

std::string Utf8(const fs::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

void TestCaseCheck() {
    const std::string dir = Scratch() + "_case";
    std::error_code ec;
    fs::create_directories(os::Path((dir + "/CaseDir").c_str()), ec);
    Touch(dir + "/CaseDir/File.DAT");
    const bool folds = fs::exists(os::Path((dir + "/casedir/FILE.dat").c_str()), ec);
    const std::string disk = Utf8(fs::path(os::Path(dir.c_str())) / "CaseDir" / "File.DAT");
    os::CaseCheckSink(CaseSinkForTest);

    // Off: nothing, whatever is asked.
    os::CaseCheckForTest(false);
    CHECK(!os::CaseCheckOn());
    (void)os::Path((dir + "/casedir/FILE.dat").c_str());
    os::WalkDirectory((dir + "/CaseDir/*.dat").c_str(), [](const os::DirEntry&) { return true; });
    CHECK(g_case_lines.empty());

    os::CaseCheckForTest(true);
    CHECK(os::CaseCheckOn());
    // The disk's own spelling: no line.
    CHECK(os::IsFile((dir + "/CaseDir/File.DAT").c_str()));
    CHECK(CaseLinesWith("File.DAT") == 0);
    // Another spelling: one line naming both, through Path (IsFile), and once only.
    CHECK(os::IsFile((dir + "\\casedir\\FILE.dat").c_str()) == folds);
    CHECK(CaseLinesWith("FILE.dat") == 1);
    CHECK(CaseLinesWith(" found " + disk) == 1);
    (void)os::Path((dir + "\\casedir\\FILE.dat").c_str());
    CHECK(CaseLinesWith("FILE.dat") == 1);
    // A part that is not there ends the check: no line.
    (void)os::Path((dir + "/casedir/NOPE.DAT").c_str());
    CHECK(CaseLinesWith("NOPE.DAT") == 0);
#ifdef _WIN32
    // A short alias of a folder is another name, not another case: no line for
    // it, and a part after it is still checked (a CI runner's TEMP holds the
    // part RUNNER~1, which failed this suite once). Where the volume makes no
    // short names the alias is the long name and the checks hold too.
    {
        const fs::path longdir = fs::path(os::Path(dir.c_str())) / L"LongAliasFolderName";
        fs::create_directories(longdir, ec);
        std::ofstream(longdir / L"Alias.BIN", std::ios::binary) << "x";
        wchar_t shortdir[MAX_PATH];
        const DWORD n = GetShortPathNameW(longdir.c_str(), shortdir, MAX_PATH);
        CHECK(n > 0 && n < MAX_PATH);
        const fs::path alias(shortdir);
        CHECK(os::IsFile((alias / L"Alias.BIN").c_str()));
        CHECK(CaseLinesWith("Alias.BIN") == 0);
        CHECK(os::IsFile((alias / L"alias.bin").c_str()));
        CHECK(CaseLinesWith("alias.bin") == 1);
        CHECK(CaseLinesWith(" found " + Utf8(alias / L"Alias.BIN")) == 1);
    }
#endif
    // The game's fopen road.
    os::CaseCheckNarrow((dir + "/CASEDIR/File.DAT").c_str());
    CHECK(CaseLinesWith("CASEDIR") == 1);
    // The game's fopen (os/narrow_open.h): a backslash is a separator on both
    // systems; a name in another case opens only where the filesystem folds,
    // and the check names it either way.
    if (std::FILE* f = os::OpenNarrow((dir + "\\CaseDir\\File.DAT").c_str(), "rb")) {
        CHECK(std::fgetc(f) == 'x');
        std::fclose(f);
    } else {
        CHECK(!"OpenNarrow of the disk's own spelling");
    }
    CHECK(os::OpenNarrow((dir + "\\CaseDir\\NOPE.DAT").c_str(), "rb") == nullptr);
    std::FILE* folded = os::OpenNarrow((dir + "\\caseDIR\\File.DAT").c_str(), "rb");
    CHECK((folded != nullptr) == folds);
    if (folded) std::fclose(folded);
    CHECK(CaseLinesWith("caseDIR") == 1);
    // A wide name's backslash, read as the seam reads a narrow one's.
    CHECK(os::PathOfWide(L"a\\b") == fs::path(L"a") / L"b");
    // A wide name (common/home.cpp's road): only a name that is there is checked.
    const fs::path wide = fs::path(os::Path(dir.c_str())) / L"CaseDir" / L"file.dat";
    CHECK(os::IsFile(wide.wstring().c_str()) == folds);
    CHECK(CaseLinesWith("file.dat") == (folds ? 1u : 0u));
    // The walk: a leaf pattern that matches only without case.
    std::size_t walked = os::WalkDirectory((dir + "/CaseDir/*.DAT").c_str(), [](const os::DirEntry&) { return true; });
    CHECK(walked == 1 && CaseLinesWith("CASECHECK walk") == 0);
    walked = os::WalkDirectory((dir + "/CaseDir/*.dat").c_str(), [](const os::DirEntry&) { return true; });
    CHECK(walked == 1 && CaseLinesWith("CASECHECK walk") == 1 && CaseLinesWith("*.dat found File.DAT") == 1);
    // The handle search (the save list's road), its directory part and its match.
    // On a case-sensitive filesystem the folder is not there by that name, so
    // nothing is found; the line for the directory part comes all the same.
    os::FindData fd{};
    const std::int32_t h = os::FindFirst((dir + "/casedir/F*.dat").c_str(), &fd);
    if (folds) {
        CHECK(h != -1 && std::strcmp(fd.name, "File.DAT") == 0);
        CHECK(CaseLinesWith("F*.dat found File.DAT") == 1);
    } else {
        CHECK(h == -1);
    }
    if (h != -1) CHECK(os::FindClose(h) == 0);
    CHECK(CaseLinesWith("asked " + dir + "/casedir/F*.dat") == 1);
    // Lines made before a sink is set are kept and handed when it is.
    os::CaseCheckSink(nullptr);
    (void)os::Path((dir + "/CaseDir/FiLe.DaT").c_str());
    CHECK(CaseLinesWith("FiLe.DaT") == 0);
    os::CaseCheckSink(CaseSinkForTest);
    CHECK(CaseLinesWith("FiLe.DaT") == 1);
    // The system's last error is the one the call before the check left.
#ifdef _WIN32
    SetLastError(1234);
    (void)os::Path((dir + "/CaseDir/fILE.DAT").c_str());
    CHECK(GetLastError() == 1234);
#else
    errno = EDOM;
    (void)os::Path((dir + "/CaseDir/fILE.DAT").c_str());
    CHECK(errno == EDOM);
#endif
    CHECK(CaseLinesWith("fILE.DAT") == 1);

    os::CaseCheckForTest(false);
    os::CaseCheckSink(nullptr);
    std::printf("case check: %u lines on a %s filesystem\n", static_cast<unsigned>(g_case_lines.size()),
                folds ? "case-insensitive" : "case-sensitive");
    fs::remove_all(os::Path(dir.c_str()), ec);
}

// ---- the launcher's seam (C7: os/process.h, os/wide_file.h, os/wide_text.h) ------
// A child started from this program itself, in child mode (C7ChildMain): its
// exit code, its output read from the pipe (stdout and stderr), a variable
// passed for it alone and one it inherits, its arguments, and its working
// folder under a name outside ASCII. Then the wide-name files and the wide
// text as Windows' converters make it.
}  // namespace
namespace {

// The child: what it was given, on its stdout and stderr, and exit code 7.
int C7ChildMain(int argc, char** argv) {
    char v[64];
    std::printf("passed=%s\n", os::GetEnv("BOF3X_C7_PASSED", v, sizeof v) ? v : "(none)");
    std::printf("inherited=%s\n", os::GetEnv("BOF3X_C7_INHERITED", v, sizeof v) ? v : "(none)");
    std::printf("removed=%s\n", os::GetEnv("BOF3X_C7_REMOVED", v, sizeof v) ? v : "(none)");
    for (int i = 1; i < argc; ++i) std::printf("arg=%s\n", argv[i]);
    std::error_code ec;
    const std::u8string leaf = std::filesystem::current_path(ec).filename().u8string();
    std::printf("folder=%s\n", std::string(leaf.begin(), leaf.end()).c_str());
    std::fflush(stdout);
    std::fprintf(stderr, "on stderr\n");
    return 7;
}

bool C7IsChild() {
    char v[8];
    return os::GetEnv("BOF3X_C7_CHILD", v, sizeof v) > 0;
}

// Stage P6's relay (ChildSpec::own_output): started with its output on a
// pipe, it starts the child above with own_output and no pipe of its own, as
// the player's launcher hands a hosted run to the developer's, and ends with
// the child's exit code. The child's lines must reach the relay's pipe. On
// Windows the child is os_tests_gui.exe, this file built as a windowed program
// (-mwindows, as the developer launcher is): a console program finds its
// parent's handles without being given them, a windowed one does not, so only
// a windowed child shows that own_output hands them over.
bool C7IsRelay() {
    char v[8];
    return os::GetEnv("BOF3X_C7_RELAY", v, sizeof v) > 0;
}

int C7RelayMain() {
    std::string self8;
    if (!os::ExecutablePath(&self8)) return 90;
    std::printf("relay: redirected=%d\n", os::OutputRedirected() ? 1 : 0);
    std::fflush(stdout);
    std::wstring program = os::ToWide(self8.c_str());
#ifdef _WIN32
    program = program.substr(0, program.find_last_of(L"\\/") + 1) + L"os_tests_gui.exe";
#endif
    os::ChildSpec spec;
    spec.program = program;
    spec.args = {L"relayed"};
    spec.env = {{L"BOF3X_C7_RELAY", L""}, {L"BOF3X_C7_CHILD", L"1"}};
    spec.own_output = true;
    os::Child child;
    std::uint32_t error = 0, code = 91;
    if (!os::ChildStart(spec, &child, &error)) return 92;
    if (!os::ChildWait(child, &code)) code = 93;
    os::ChildClose(child);
    return static_cast<int>(code);
}

void TestProcess() {
    namespace fs = std::filesystem;
    std::string self8;
    CHECK(os::ExecutablePath(&self8));
    const std::wstring self = os::ToWide(self8.c_str());
    std::error_code ec;
    // the working folder: "c7_" and e-acute, CJK and Cyrillic letters
    const fs::path dir = fs::temp_directory_path(ec) / fs::path(L"bof3x_c7_é中Ж");
    fs::create_directories(dir, ec);
    CHECK(fs::is_directory(dir, ec));

    CHECK(os::SetEnvW(L"BOF3X_C7_INHERITED", L"yes"));
    CHECK(os::SetEnv("BOF3X_C7_REMOVED", "there"));
    os::ChildSpec spec;
    spec.program = self;
    // a folder's trailing separator and a double quote come back as given (stage P4's paths)
    spec.args = {L"one", L"two words", L"D:\\", L"a \\\\ b\\\\", L"say \"hi\""};
    spec.directory = dir.wstring();
    spec.env = {{L"BOF3X_C7_CHILD", L"1"}, {L"BOF3X_C7_PASSED", L"c7-value"}, {L"BOF3X_C7_REMOVED", L""}};
    spec.capture = true;
    spec.no_window = true;
    os::Child child;
    std::uint32_t error = 0;
    const bool started = os::ChildStart(spec, &child, &error);
    CHECK(started);
    std::string out;
    if (started) {
        char buf[256];
        for (std::int64_t n; (n = os::ChildRead(child, buf, sizeof buf)) > 0;) out.append(buf, static_cast<std::size_t>(n));
        std::uint32_t code = 0;
        CHECK(os::ChildWait(child, &code));
        CHECK(code == 7);
        os::ChildClose(child);
    }
    const auto has = [&out](const std::string& line) { return out.find(line + "\n") != std::string::npos ||
                                                             out.find(line + "\r\n") != std::string::npos; };
    CHECK(has("passed=c7-value"));
    CHECK(has("inherited=yes"));
    CHECK(has("removed=(none)"));
    CHECK(has("arg=one"));
    CHECK(has("arg=two words"));
    CHECK(has("arg=D:\\"));
    CHECK(has("arg=a \\\\ b\\\\"));
    CHECK(has("arg=say \"hi\""));
    CHECK(has("folder=bof3x_c7_\xC3\xA9\xE4\xB8\xAD\xD0\x96"));
    CHECK(has("on stderr"));
    char v[8];
    CHECK(os::GetEnv("BOF3X_C7_PASSED", v, sizeof v) == 0);   // the child's alone
    CHECK(os::SetEnvW(L"BOF3X_C7_INHERITED", nullptr));
    CHECK(os::GetEnv("BOF3X_C7_INHERITED", v, sizeof v) == 0);
    CHECK(os::SetEnv("BOF3X_C7_REMOVED", nullptr));

    // no output asked: ChildRead refuses; the exit code still comes back
    os::ChildSpec quiet = spec;
    quiet.capture = false;
    quiet.args = {};
    quiet.env = {{L"BOF3X_C7_CHILD", L"1"}};
    std::FILE* keep = nullptr;
    (void)keep;
    if (os::ChildStart(quiet, &child, &error)) {
        char buf[8];
        CHECK(os::ChildRead(child, buf, sizeof buf) == -1);
        std::uint32_t code = 0;
        CHECK(os::ChildWait(child, &code) && code == 7);
        os::ChildClose(child);
    } else {
        CHECK(!"the child without a pipe started");
    }

    // own_output (stage P6): a relay on a pipe hands its pipe to its own child, which writes there
    {
        os::ChildSpec relay;
        relay.program = self;
        relay.env = {{L"BOF3X_C7_RELAY", L"1"}};
        relay.capture = true;
        relay.no_window = true;
        std::string got;
        if (os::ChildStart(relay, &child, &error)) {
            char buf[256];
            for (std::int64_t n; (n = os::ChildRead(child, buf, sizeof buf)) > 0;) got.append(buf, static_cast<std::size_t>(n));
            std::uint32_t code = 0;
            CHECK(os::ChildWait(child, &code) && code == 7);
            os::ChildClose(child);
        } else {
            CHECK(!"the relay started");
        }
        const auto saw = [&got](const std::string& line) { return got.find(line + "\n") != std::string::npos ||
                                                                  got.find(line + "\r\n") != std::string::npos; };
        CHECK(saw("relay: redirected=1"));   // a pipe is redirected output
        CHECK(saw("arg=relayed"));           // the relay's child wrote on the relay's pipe, stdout ...
        CHECK(saw("on stderr"));             // ... and stderr
    }
    std::printf("process: own_output - a child writes on its parent's pipe and its exit code comes back\n");

    // a program that is not there: no child, the system's error number
    os::ChildSpec none;
    none.program = (dir / L"no_such_program.exe").wstring();
    error = 0;
    CHECK(!os::ChildStart(none, &child, &error));
    CHECK(error != 0);
#ifndef _WIN32
    os::ChildSpec held = spec;
    held.suspended = true;
    CHECK(!os::ChildStart(held, &child, &error) && error == EINVAL);   // no suspended start on POSIX
#endif
    std::printf("process: a child's exit code, its output on the pipe, its environment, its arguments, a folder "
                "outside ASCII\n");

    // ---- files under a wide name ----
    const std::wstring wdir = dir.wstring();
    const wchar_t sep = static_cast<wchar_t>(fs::path::preferred_separator);
    const std::wstring file = wdir + sep + L"Layer.AREA001.DAT";
    CHECK(os::WriteWholeW(file, std::string("0123456789")));
    CHECK(os::IsFileW(file) && !os::IsDirectoryW(file));
    CHECK(os::IsDirectoryW(wdir) && !os::IsFileW(wdir));
    CHECK(!os::IsFileW(wdir + sep + L"absent"));
    CHECK(os::AnyMatchW(wdir + sep + L"layer.*.dat"));   // the leaf's ASCII case does not count, as FindFirstFile
    CHECK(os::AnyMatchW(wdir + L"\\Layer.AREA00?.DAT"));   // a backslash is a separator on both systems
    CHECK(!os::AnyMatchW(wdir + sep + L"other.*"));
    fs::create_directories(dir / L"sub", ec);
    const std::vector<os::MatchW> m = os::MatchesW(wdir + sep + L"*");
    bool saw_file = false, saw_dir = false;
    std::size_t named = 0;   // Windows hands "." and ".." to "*" as FindFirstFileW does; the walk never does
    for (const os::MatchW& e : m) {
        saw_file = saw_file || (e.name == L"Layer.AREA001.DAT" && !e.directory);
        saw_dir = saw_dir || (e.name == L"sub" && e.directory);
        named += e.name != L"." && e.name != L"..";
    }
    CHECK(named == 2 && saw_file && saw_dir);
    const os::InFile in = os::InOpenW(file, true);
    CHECK(in != os::kNoInFile);
    if (in != os::kNoInFile) {
        std::uint64_t size = 0;
        CHECK(os::InSize(in, &size) && size == 10);
        char b[16] = {};
        std::uint32_t got = 0;
        CHECK(os::InSeek(in, 4) && os::InRead(in, b, 3, &got) && got == 3 && std::memcmp(b, "456", 3) == 0);
        CHECK(os::InRead(in, b, 16, &got) && got == 3);
        CHECK(os::InRead(in, b, 16, &got) && got == 0);   // its end
        os::InClose(in);
    }
    CHECK(os::InOpenW(wdir + sep + L"absent", false) == os::kNoInFile);
    std::uint64_t size = 0, time = 0, time2 = 0;
    CHECK(os::StampW(file, &size, &time) && size == 10);
    CHECK(os::WriteWholeW(file, std::string("abc")));
    CHECK(os::StampW(file, &size, &time2) && size == 3);
    CHECK(!os::StampW(wdir + sep + L"absent", &size, &time));
    std::wstring full;
    CHECK(os::FullPathW(wdir + sep + L"sub" + sep + L".." + sep + L"Layer.AREA001.DAT", &full) && full == file);
    CHECK(os::FullPathW(L"rel", &full) && full.size() > 3 && full.substr(full.size() - 4) == std::wstring(1, sep) + L"rel");
    CHECK(!os::FullPathW(wdir + sep + std::wstring(300, L'x'), &full));
    fs::remove_all(dir, ec);
    std::printf("wide files: open, read, seek, size, write, stamp, match, full name\n");

    // ---- wide text ----
    CHECK(os::WideOfUtf8("a\xC3\xA9\xE4\xB8\xAD") == L"aé中");
    CHECK(os::Utf8OfWide(L"aé中") == "a\xC3\xA9\xE4\xB8\xAD");
    CHECK(os::Utf8OfWide(os::WideOfUtf8("\xF0\x9F\x98\x80")) == "\xF0\x9F\x98\x80");   // beyond the BMP, both ways
    CHECK(os::WideOfUtf8("a\xFF" "b") == L"a�" L"b");         // a byte that is no UTF-8
    CHECK(os::WideOfUtf8("\xE4\xB8") == L"�");                // cut short at the end
    CHECK(os::Utf8OfWide(std::wstring(1, static_cast<wchar_t>(0xD83D))) == "\xEF\xBF\xBD");   // a lone surrogate
    CHECK(os::IsUtf8("plain") && os::IsUtf8("\xC3\xA9") && os::IsUtf8("\xEF\xBF\xBD"));
    CHECK(!os::IsUtf8("") && !os::IsUtf8("\xC3") && !os::IsUtf8("\xC0\x80") && !os::IsUtf8("\xED\xA0\x80"));
#ifdef _WIN32
    // WideOfMessage is the C runtime's own %hs in the "C" locale, byte for byte
    std::string all;
    for (int c = 1; c < 256; ++c) all += static_cast<char>(c);
    wchar_t hs[512];
    std::swprintf(hs, 512, L"%hs", all.c_str());
    CHECK(os::WideOfMessage(all) == hs);
#else
    CHECK(os::WideOfMessage("\xC3\xA9") == L"é");
    CHECK(os::WideOfAnsi("\xE9") == L"é");   // no ANSI page here: Latin-1
#endif
    std::printf("wide text: UTF-8 and wide both ways, U+FFFD for what does not convert\n");
}

}  // namespace

// ---- what runs (os/running.h; stage P5's "a game is running") ---------------------
// A stand-in: a copy of this program under another file name, started in its
// stand-in mode (BOF3X_RUNNING_STANDIN=<ms>: it waits that long and ends; with
// BOF3X_RUNNING_CLASS=<name> it first opens a hidden window of that class on
// Windows, as the game registers "Bof3"). The seam must count its process
// while it runs and not once it has ended, under a short file name and under
// one longer than the 15 bytes Linux keeps of a process's name. Then the
// game's own marks are asked, and the answer said: true there is a true answer
// when the player's game runs.
namespace {

int StandInMain() {
    char v[64];
    const int ms = os::GetEnv("BOF3X_RUNNING_STANDIN", v, sizeof v) ? std::atoi(v) : 0;
#ifdef _WIN32
    if (os::GetEnv("BOF3X_RUNNING_CLASS", v, sizeof v) > 0) {
        WNDCLASSA wc = {};
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = v;
        if (RegisterClassA(&wc)) CreateWindowExA(0, v, "", 0, 0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return 0;
}

bool StandInAsked() {
    char v[8];
    return os::GetEnv("BOF3X_RUNNING_STANDIN", v, sizeof v) > 0;
}

// Polls `ok` for up to 5 s: a process or a window the system lists a moment after it starts or ends.
template <class F>
bool Within5s(F ok) {
    for (int i = 0; i < 100; ++i) {
        if (ok()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

void TestRunning() {
    namespace fs = std::filesystem;
    std::string self8;
    CHECK(os::ExecutablePath(&self8));
    const fs::path self = fs::path(os::ToWide(self8.c_str()));
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / "bof3x_running";
    fs::create_directories(dir, ec);
#ifdef _WIN32
    const unsigned id = static_cast<unsigned>(GetCurrentProcessId());
    const std::string ending = ".exe";
#else
    const unsigned id = static_cast<unsigned>(getpid());
    const std::string ending;
#endif
    CHECK(os::ProgramsRunning(self.filename().string().c_str()) >= 1);   // this program itself
    CHECK(os::ProgramsRunning(("bof3x_no_such_program_" + std::to_string(id) + ending).c_str()) == 0);
#ifndef _WIN32
    CHECK(os::ProgramsRunning("a/b") == -1 && os::LastError() == EINVAL);   // a file name, not a path
#endif
    const std::string names[2] = {"b3r" + std::to_string(id % 100000) + ending,
                                  "bof3x_running_stand_in_" + std::to_string(id) + ending};
    const std::string cls = "bof3x_running_class_" + std::to_string(id);
    for (const std::string& name : names) {
        const fs::path copy = dir / name;
        fs::copy_file(self, copy, fs::copy_options::overwrite_existing, ec);
        CHECK(!ec);
        os::ChildSpec spec;
        spec.program = copy.wstring();
        spec.env = {{L"BOF3X_RUNNING_STANDIN", L"20000"}, {L"BOF3X_RUNNING_CLASS", os::ToWide(cls.c_str())}};
        spec.no_window = true;
        os::Child child;
        std::uint32_t error = 0;
        const bool started = os::ChildStart(spec, &child, &error);
        CHECK(started);
        if (!started) continue;
        CHECK(Within5s([&] { return os::ProgramsRunning(name.c_str()) == 1; }));
#ifdef _WIN32
        std::string upper = name;   // Windows does not count the case of a file name
        for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        CHECK(os::ProgramsRunning(upper.c_str()) == 1);
        CHECK(Within5s([&] { return os::WindowClassOpen(cls.c_str()); }));
#else
        CHECK(!os::WindowClassOpen(cls.c_str()) && os::LastError() == ENOSYS);
#endif
        CHECK(os::ChildEnd(child, 1));
        std::uint32_t code = 0;
        CHECK(os::ChildWait(child, &code));
        os::ChildClose(child);
        CHECK(Within5s([&] { return os::ProgramsRunning(name.c_str()) == 0; }));
#ifdef _WIN32
        CHECK(!os::WindowClassOpen(cls.c_str()));
#endif
        fs::remove(copy, ec);
    }
    fs::remove_all(dir, ec);
    // The game's own marks, as the save editor asks for them: said, not checked (the player's game may run).
#ifdef _WIN32
    const char* game = "bof3x.exe";
    const int windows = os::WindowClassOpen("Bof3") ? 1 : 0;
#else
    const char* game = "bof3x";
    const int windows = -1;
#endif
    const int games = os::ProgramsRunning(game);
    CHECK(games >= 0);
    std::printf("running: a stand-in counted while it ran and not after, under a short and a long name; %s: %d "
                "process(es), a window of the class Bof3: %s\n",
                game, games, windows < 0 ? "not asked (no window classes here)" : windows ? "yes" : "no");
}

}  // namespace

int main(int argc, char** argv) {
    if (C7IsRelay()) return C7RelayMain();               // TestProcess's relay (stage P6)
    if (C7IsChild()) return C7ChildMain(argc, argv);   // TestProcess's child
    if (StandInAsked()) return StandInMain();           // TestRunning's stand-in
    TestMemory();
    TestFiles();
    TestNarrowNames();
    TestFindAndStreams();
    TestEventsThreadsFiles();
    TestClocks();
    TestEnvironment();
    TestSha256();
    TestStrTo32();
    TestHeapArena();
    TestUtf8Paths();
    TestR1Harness();
    TestOwnStack();
    TestFileMove();
    TestWindowKey();
    TestCaseCheck();
    TestProcess();
    TestRunning();
    std::printf("os_tests: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
