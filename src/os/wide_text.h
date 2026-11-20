// bof3::os, wide text as Windows' converters make it: UTF-8 and the ANSI code
// page to wchar_t and back, and wide text written to a C stream. For the
// launcher's settings model and its start of the game (launcher/config.cpp,
// launcher/start.cpp), whose texts reach the player's screen and bof3x.ini.
//
// os/text.h's ToUtf8 / ToWide go through std::filesystem::path and come back
// empty for text that does not convert. These convert every text, as
// MultiByteToWideChar and WideCharToMultiByte with no flags do on Windows: a
// byte sequence that is not UTF-8, and a UTF-16 surrogate without its pair,
// become U+FFFD. POSIX keeps that rule with its 4-byte wchar_t: a wchar_t is a
// code point there, and one that is a surrogate or beyond U+10FFFF is U+FFFD.
// So a UTF-16 pair written as two wchar_t (a JSON "😀", one half at
// a time) is two U+FFFD on both systems.
//
// Windows: MultiByteToWideChar and WideCharToMultiByte (CP_UTF8; CP_ACP for
// the ANSI page), and std::fwprintf for a stream. POSIX: our own UTF-8 code;
// there is no ANSI code page, so WideOfAnsi reads each byte as its Latin-1
// character; and a stream gets the text's UTF-8 through std::fputs, so a
// stream is never wide-oriented (glibc refuses a narrow write to a stream a
// wide write has oriented, and the launcher's model writes narrow lines).
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdio>
#include <string>

namespace bof3::os {

std::wstring WideOfUtf8(const std::string& utf8);
std::string Utf8OfWide(const std::wstring& wide);
// Whether the bytes are well-formed UTF-8 (MultiByteToWideChar with
// MB_ERR_INVALID_CHARS takes them): no overlong form, no surrogate, nothing
// past U+10FFFF. Empty is not.
bool IsUtf8(const std::string& bytes);
// The ANSI code page's bytes as wide text (POSIX: Latin-1).
std::wstring WideOfAnsi(const std::string& ansi);

// A narrow message text (a refusal's `why`, a hash, a list of keys) as wide
// text for a wide message, as a "%hs" of the launcher's wide formats widened
// it before the seam. Windows: each byte its own character, which is what the
// C runtime's %hs does in the "C" locale (the launcher never sets another);
// POSIX: the text's UTF-8 (WideOfUtf8).
std::wstring WideOfMessage(const std::string& narrow);

// `text` written to `stream` as it is, no new line added. Windows:
// std::fwprintf(stream, L"%ls", text), so the C runtime's own conversion
// writes it as a wide format always did; POSIX: its UTF-8.
void PrintW(std::FILE* stream, const std::wstring& text);

}  // namespace bof3::os
