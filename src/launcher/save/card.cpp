// The memory card image and the PlayStation layout: see card.h. The format
// is docs/save-editor-spec.md section 5.2; what was measured for the parts
// written here (the empty card, the directory frame, the two titles) is in
// docs/save-editor.md section 7.
#include "launcher/save/card.h"

#include <cstring>
#include <filesystem>
#include <fstream>

namespace bof3x::save {

const char kCardPrefixUs[] = "BASLUS-00422BOF3";
const char kCardPrefixJp[] = "BISLPS-00990BOF3";

namespace {

constexpr std::size_t kFrame = 0x80;       // a directory frame
constexpr std::size_t kCardBlock = 0x2000; // a card block: block n is at n * 0x2000
constexpr std::size_t kHeader = 0x200;     // "SC", the title and the icon, before the game's block
constexpr std::size_t kTitle = 0x04, kTitleSize = 0x40;

constexpr std::uint32_t kChars = 0x90, kCharStride = 0xA4, kCharCount = 8;
constexpr std::uint32_t kPsxName = 5, kPcName = 9, kShift = kPcName - kPsxName;
constexpr std::uint32_t kClock = 0x6E8, kSummaryLevel = 0xCA8;

std::uint8_t Xor(const std::uint8_t* p, std::size_t n) {
    std::uint8_t v = 0;
    for (std::size_t i = 0; i < n; ++i) v ^= p[i];
    return v;
}

// A directory frame's state: 0x51 starts a save, 0x52 and 0x53 continue one,
// 0xA0 is free and 0xA1..0xA3 are the frames of a deleted save.
bool FrameFree(const Card& card, int slot) { return (card[static_cast<std::size_t>(slot) * kFrame] & 0xF0) == 0xA0; }

std::string FrameName(const Card& card, int slot) {
    const std::uint8_t* p = card.data() + static_cast<std::size_t>(slot) * kFrame + 0x0A;
    std::string name;
    for (int i = 0; i < 20 && p[i]; ++i) name += static_cast<char>(p[i]);
    return name;
}

// A two-byte Shift-JIS character.
void Wide(std::uint8_t*& p, unsigned code) {
    *p++ = static_cast<std::uint8_t>(code >> 8);
    *p++ = static_cast<std::uint8_t>(code);
}
// Two full-width digits (0x824F is the full-width 0).
void Digits(std::uint8_t*& p, unsigned v) {
    v = v > 99 ? 99 : v;
    Wide(p, 0x824F + v / 10);
    Wide(p, 0x824F + v % 10);
}

// The title a console's card screen shows; the game does not read it. The
// game writes one of two forms, measured on 28 saves:
//   US  "Breath of Fire 3(n) HHhMMm Lv.LL", full-width, all 64 bytes
//   JP  the game's name in katakana, "3(n) HH<hours>MM<minutes> <level>LL", 58 bytes
// with n the file name's number + 1, the play time, and the summary's level.
void WriteTitle(std::uint8_t* body, bool japanese, int number, const std::uint8_t* psx_block) {
    std::uint8_t* p = body + kTitle;
    std::memset(p, 0, kTitleSize);
    const unsigned hours = psx_block[kClock], minutes = psx_block[kClock + 1], level = psx_block[kSummaryLevel];
    if (japanese) {
        for (const unsigned c : {0x8375u, 0x838Cu, 0x8358u, 0x8145u, 0x8349u, 0x8375u, 0x8145u, 0x8374u, 0x8340u,
                                 0x8343u, 0x8341u, 0x8252u, 0x8169u})
            Wide(p, c);
        Wide(p, 0x8250 + static_cast<unsigned>(number));
        Wide(p, 0x816A);
        Wide(p, 0x8140);
        Digits(p, hours);
        Wide(p, 0x8E9E);
        Wide(p, 0x8AD4);
        Digits(p, minutes);
        Wide(p, 0x95AA);
        Wide(p, 0x8140);
        for (const unsigned c : {0x838Cu, 0x8378u, 0x838Bu}) Wide(p, c);
        Digits(p, level);
    } else {
        for (const unsigned c : {0x8261u, 0x8292u, 0x8285u, 0x8281u, 0x8294u, 0x8288u, 0x8140u, 0x828Fu, 0x8286u,
                                 0x8140u, 0x8265u, 0x8289u, 0x8292u, 0x8285u, 0x8140u, 0x8252u, 0x8169u})
            Wide(p, c);
        Wide(p, 0x8250 + static_cast<unsigned>(number));
        Wide(p, 0x816A);
        Wide(p, 0x8140);
        Digits(p, hours);
        Wide(p, 0x8288);
        Digits(p, minutes);
        Wide(p, 0x828D);
        Wide(p, 0x8140);
        for (const unsigned c : {0x826Bu, 0x8296u, 0x8144u}) Wide(p, c);
        Digits(p, level);
    }
}

// The icon of a save that has no save of this game to take one from: drawn
// here, one still frame, a flame in a ring. None of the game's pixels.
void OwnIcon(std::uint8_t* body) {
    body[2] = 0x11;   // one frame
    const std::uint16_t colours[5] = {0x0000, 0x4000 | (2 << 5) | 2, 0x7FFF, (10 << 5) | 31, (24 << 5) | 31};
    for (int i = 0; i < 5; ++i) {
        body[0x60 + 2 * i] = static_cast<std::uint8_t>(colours[i]);
        body[0x61 + 2 * i] = static_cast<std::uint8_t>(colours[i] >> 8);
    }
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const int dx = 2 * x - 15, dy = 2 * y - 15, r = dx * dx + dy * dy;
            int c = 0;
            if (r <= 15 * 15 + 30) c = r >= 12 * 12 ? 2 : 1;
            // The flame: a drop, wide at the base, with a lighter core.
            const int fy = y - 3, half = fy < 0 ? -1 : (fy < 5 ? fy / 2 + 1 : (fy < 9 ? 3 : 10 - fy));
            const int fx = dx < 0 ? -dx : dx;
            if (r < 12 * 12 && fy >= 0 && fy <= 9 && fx <= 2 * half) c = fx <= half && fy >= 4 ? 4 : 3;
            std::uint8_t& b = body[0x80 + y * 8 + x / 2];
            b = static_cast<std::uint8_t>(x & 1 ? (b & 0x0F) | (c << 4) : (b & 0xF0) | c);
        }
}

std::uint8_t* Body(Card& card, int slot) { return card.data() + static_cast<std::size_t>(slot) * kCardBlock; }
const std::uint8_t* Body(const Card& card, int slot) { return card.data() + static_cast<std::size_t>(slot) * kCardBlock; }

bool IsJapanese(const std::string& prefix) { return prefix.size() >= 2 && prefix[1] == 'I'; }

bool LiveBof3(const Card& card, int slot, CardEntry* out) {
    if (slot < 1 || slot > kCardSlots) return false;
    for (const CardEntry& e : CardEntries(card))
        if (e.slot == slot && e.bof3) {
            if (out) *out = e;
            return true;
        }
    return false;
}

}  // namespace

Card EmptyCard() {
    Card card(kCardSize, 0);
    card[0] = 'M';
    card[1] = 'C';
    card[0x7F] = 0x0E;
    for (int n = 1; n <= kCardSlots; ++n) {
        std::uint8_t* f = card.data() + static_cast<std::size_t>(n) * kFrame;
        f[0] = 0xA0;
        f[8] = f[9] = 0xFF;
        f[0x7F] = Xor(f, 0x7F);
    }
    for (int n = 16; n < 36; ++n) {   // the list of replaced sectors: none
        std::uint8_t* f = card.data() + static_cast<std::size_t>(n) * kFrame;
        std::memset(f, 0xFF, 4);
        f[8] = f[9] = 0xFF;
    }
    std::memcpy(card.data() + 63 * kFrame, card.data(), kFrame);
    return card;
}

bool ReadCardFile(const std::wstring& path, Card& card, std::string& why) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) {
        why = "The file cannot be opened.";
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 2 && static_cast<std::uint8_t>(bytes[0]) == 0x1F && static_cast<std::uint8_t>(bytes[1]) == 0x8B) {
        why = "This is a save state of an emulator, not a memory card file. The editor cannot open it.";
        return false;
    }
    if (bytes.size() != kCardSize || bytes[0] != 'M' || bytes[1] != 'C') {
        why = "This is not a memory card file that the editor can open. The editor opens the card files of 131072 "
              "bytes that Mednafen, RetroArch and other emulators write.";
        return false;
    }
    card.assign(bytes.begin(), bytes.end());
    return true;
}

bool WriteCardFile(const std::wstring& path, const Card& card, const std::wstring& backup_dir, std::wstring& backup,
                   std::string& why) {
    if (card.size() != kCardSize) {
        why = "The memory card is not complete. The file did not change.";
        return false;
    }
    return WriteKept(path, std::vector<char>(card.begin(), card.end()), backup_dir, backup, why);
}

std::vector<CardEntry> CardEntries(const Card& card) {
    std::vector<CardEntry> out;
    if (card.size() != kCardSize) return out;
    for (int n = 1; n <= kCardSlots; ++n) {
        const std::uint8_t* f = card.data() + static_cast<std::size_t>(n) * kFrame;
        if (f[0] != 0x51) continue;
        CardEntry e;
        e.slot = n;
        e.name = FrameName(card, n);
        const std::size_t len = e.name.size();
        const bool one_block = f[8] == 0xFF && f[9] == 0xFF;
        if (one_block && len >= 6 && e.name.compare(len - 6, 4, "BOF3") == 0 && e.name[len - 2] >= '0' &&
            e.name[len - 2] <= '9' && e.name[len - 1] >= '0' && e.name[len - 1] <= '9' && Body(card, n)[0] == 'S' &&
            Body(card, n)[1] == 'C') {
            e.bof3 = true;
            e.prefix = e.name.substr(0, len - 2);
            e.number = (e.name[len - 2] - '0') * 10 + (e.name[len - 1] - '0');
        }
        out.push_back(std::move(e));
    }
    return out;
}

int CardFreeSlot(const Card& card) {
    if (card.size() != kCardSize) return 0;
    for (int n = 1; n <= kCardSlots; ++n)
        if (FrameFree(card, n)) return n;
    return 0;
}

int CardFreeNumber(const Card& card, const std::string& prefix) {
    bool used[kCardSavesPerRelease] = {};
    for (const CardEntry& e : CardEntries(card))
        if (e.bof3 && e.prefix == prefix && e.number >= 0 && e.number < kCardSavesPerRelease) used[e.number] = true;
    for (int n = 0; n < kCardSavesPerRelease; ++n)
        if (!used[n]) return n;
    return -1;
}

const std::uint8_t* CardHeader(const Card& card, int slot) {
    return card.size() == kCardSize && slot >= 1 && slot <= kCardSlots ? Body(card, slot) : nullptr;
}

bool PsxToPc(const std::uint8_t* psx, Block& pc, std::string& why) {
    std::memcpy(pc.data(), psx, kBlockSize);
    for (std::uint32_t i = 0; i < kCharCount; ++i) {
        const std::uint8_t* from = psx + kChars + i * kCharStride;
        std::uint8_t* to = pc.data() + kChars + i * kCharStride;
        for (std::uint32_t k = kCharStride - kShift; k < kCharStride; ++k)
            if (from[k]) {
                why = "Character record " + std::to_string(i + 1) +
                      " of this save uses bytes that the editor does not know. The editor cannot open this save.";
                return false;
            }
        std::memset(to, 0, kPcName);
        std::memcpy(to, from, kPsxName);
        std::memcpy(to + kPcName, from + kPsxName, kCharStride - kPcName);
    }
    return true;
}

bool PcToPsx(const Block& pc, std::uint8_t* psx, std::string& why) {
    std::memcpy(psx, pc.data(), kBlockSize);
    for (std::uint32_t i = 0; i < kCharCount; ++i) {
        const std::uint8_t* from = pc.data() + kChars + i * kCharStride;
        std::uint8_t* to = psx + kChars + i * kCharStride;
        for (std::uint32_t k = kPsxName; k < kPcName; ++k)
            if (from[k]) {
                why = "The name of character " + std::to_string(i + 1) +
                      " is too long for a memory card save, which holds five letters. Make the name shorter.";
                return false;
            }
        std::memcpy(to, from, kPsxName);
        std::memcpy(to + kPsxName, from + kPcName, kCharStride - kPcName);
        std::memset(to + kCharStride - kShift, 0, kShift);
    }
    return true;
}

void ClearPcOnly(Block& block) {
    block[0x1040] = 0;
    block[0x7C] = block[0x7D] = 0;
    Seal(block);
}

bool CardGet(const Card& card, int slot, Block& block, std::string& why) {
    if (!LiveBof3(card, slot, nullptr)) {
        why = "This place on the memory card has no save of this game.";
        return false;
    }
    return PsxToPc(Body(card, slot) + kHeader, block, why);
}

bool CardPut(Card& card, int slot, const Block& block, std::string& why) {
    CardEntry e;
    if (!LiveBof3(card, slot, &e)) {
        why = "This place on the memory card has no save of this game.";
        return false;
    }
    std::uint8_t psx[kBlockSize];
    if (!PcToPsx(block, psx, why)) return false;
    std::uint8_t* body = Body(card, slot);
    const std::uint8_t* old = body + kHeader;
    const bool title = old[kClock] != psx[kClock] || old[kClock + 1] != psx[kClock + 1] ||
                       old[kSummaryLevel] != psx[kSummaryLevel];
    if (title) WriteTitle(body, IsJapanese(e.prefix), e.number > 8 ? 8 : e.number, psx);
    std::memcpy(body + kHeader, psx, kBlockSize);
    return true;
}

int CardAdd(Card& card, const std::string& prefix, int number, const Block& block, const std::uint8_t* header,
            std::string& why) {
    if (card.size() != kCardSize) {
        why = "The memory card is not complete.";
        return 0;
    }
    if (number < 0 || number >= kCardSavesPerRelease || prefix.empty() || prefix.size() > 17) {
        why = "The game shows three saves on one memory card. This card has three.";
        return 0;
    }
    for (const CardEntry& e : CardEntries(card))
        if (e.bof3 && e.prefix == prefix && e.number == number) {
            why = "The memory card has a save with this name.";
            return 0;
        }
    const int slot = CardFreeSlot(card);
    if (!slot) {
        why = "The memory card is full.";
        return 0;
    }
    std::uint8_t psx[kBlockSize];
    if (!PcToPsx(block, psx, why)) return 0;

    std::uint8_t* body = Body(card, slot);
    std::memset(body, 0, kCardBlock);
    if (header) {
        std::memcpy(body, header, kHeader);
    } else {
        body[0] = 'S';
        body[1] = 'C';
        body[3] = 1;   // one card block
        OwnIcon(body);
    }
    WriteTitle(body, IsJapanese(prefix), number, psx);
    std::memcpy(body + kHeader, psx, kBlockSize);

    std::uint8_t* f = card.data() + static_cast<std::size_t>(slot) * kFrame;
    std::memset(f, 0, kFrame);
    f[0] = 0x51;
    f[5] = 0x20;   // the size, 0x2000
    f[8] = f[9] = 0xFF;
    const std::string name = prefix + static_cast<char>('0' + number / 10) + static_cast<char>('0' + number % 10);
    std::memcpy(f + 0x0A, name.data(), name.size());
    f[0x7F] = Xor(f, 0x7F);
    return slot;
}

}  // namespace bof3x::save
