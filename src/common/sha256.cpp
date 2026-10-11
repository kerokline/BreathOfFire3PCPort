#include "common/sha256.h"

#include <windows.h>

#include <bcrypt.h>

#include <cstdio>
#include <vector>

namespace bof3x {
namespace {

std::string Hex(const unsigned char (&digest)[32]) {
    static const char kHex[] = "0123456789abcdef";
    std::string hex;
    for (unsigned char b : digest) {
        hex += kHex[b >> 4];
        hex += kHex[b & 15];
    }
    return hex;
}

}  // namespace

bool Sha256Hex(const wchar_t* path, std::string& hex, std::string& why) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        why = "cannot open the file (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = false;
    unsigned char digest[32];
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        why = "cannot initialise SHA-256";
    } else {
        std::vector<unsigned char> buf(1 << 16);
        for (;;) {
            DWORD got = 0;
            if (!ReadFile(file, buf.data(), static_cast<DWORD>(buf.size()), &got, nullptr)) {
                why = "read error (error " + std::to_string(GetLastError()) + ")";
                break;
            }
            if (got == 0) {
                ok = BCryptFinishHash(hash, digest, sizeof digest, 0) >= 0;
                if (!ok) why = "SHA-256 finish failed";
                break;
            }
            if (BCryptHashData(hash, buf.data(), got, 0) < 0) {
                why = "SHA-256 update failed";
                break;
            }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(file);
    if (!ok) return false;
    hex = Hex(digest);
    return true;
}

bool Sha256HexBytes(const void* data, std::size_t size, std::string& hex) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char digest[32];
    const bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
                    BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
                    BCryptHashData(hash, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(size), 0) >= 0 &&
                    BCryptFinishHash(hash, digest, sizeof digest, 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (ok) hex = Hex(digest);
    return ok;
}

}  // namespace bof3x
