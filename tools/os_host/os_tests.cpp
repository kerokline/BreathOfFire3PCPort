// Tests of the operating-system seam (src/os/os.h) on the machine they are
// built on: every service the engine uses, with synthetic data only. Built
// with the system compiler on Linux (src/os/posix) and with llvm-mingw on
// Windows (src/os/win32), both in CI. See tools/os_host/CMakeLists.txt.
#include "common/sha256.h"
#include "os/os.h"

#include <atomic>
#include <chrono>
#include <ctime>
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
#include <fcntl.h>
#include <semaphore.h>
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
#ifdef _WIN32
        CloseHandle(other);
#else
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

int main() {
    TestMemory();
    TestFiles();
    TestNarrowNames();
    TestFindAndStreams();
    TestClocks();
    TestEnvironment();
    TestSha256();
    TestHeapArena();
    TestUtf8Paths();
    TestR1Harness();
    std::printf("os_tests: %d checks, %d failed\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}
