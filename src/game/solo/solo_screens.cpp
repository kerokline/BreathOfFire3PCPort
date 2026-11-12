#include "game/solo/solo_screens.h"

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/solo/solo.h"
#include "hook/detour.h"
#include "hook/log.h"

// The members screens' long reserve in the solo-character mode (group MS;
// solo_screens.h has the why). What each address is:
//
//   CharacterRecords     0x903A70  eight records, 0xA4 each: +9 the id,
//                                  +0xB bit 0 joined (solo-character-reads.md M6)
//   Character_IndexById  0x66972C  a member id's record
//
// The state here lives only while a members screen is open: each screen
// fills it when it builds its reserve and nothing outside the screen reads
// it. It is not in the quicksave's span; a quick-load lands on the field
// (the menu closed), where the next screen fills it anew.

namespace solo_screens {

Reserve g_tactics;
Reserve g_party_form;

bool Wide() { return solo::Solo_Active(); }

bool IsChampion(unsigned char id) {
    if (!solo::Solo_Active()) return false;
    return static_cast<int>(Character_IndexById[id]) == solo::Solo_ChampionRecord();
}

// The cleared slot's draws, read from Menu_DrawSaveSlot (src/game/menu/
// menu.cpp, 0x576960): with summary +0x14 set the slot's frame pieces
// (0x663584) go to Menu_DrawPieces with flags 2 - Menu_DrawPiece's CLUT
// (0x80, 0x1E2) in place of (0xB0, 0x1E1) (menu_2.cpp) - and the piece 0x3D
// (rectangle 0xC8, 0x30, 27 x 13 at 0x663B94 + 4 * 0x3D) at (x + 0x32,
// y + 0x16), right of the level (x + 0x1F, y + 0x18).
//
// The member status panel (Menu_DrawMemberStatus, menu.cpp) ends with its
// frame pieces Menu_MemberPieces (flags 0) and then its EXP bar, so the
// cleared look on it is those pieces again with flags 2 and the bar again
// over them. Its level is drawn "%3d" from x + 0x16, so the emblem at the
// slot's distance from the level, x + 0x2F, y + 0x15, covers the top of the HP
// line (rows 8 apart, the emblem 13 high): kChampionClearEmblem is off.
// Menu_DrawMemberStatus also sets or clears the record's +0x10 bit 13 (HP
// below a quarter) from its HP, as it does for every member it draws; for a
// champion in reserve (`whole`) that is a write the original's screen does
// not make, of a bit the next status draw sets the same way.
void DrawCleared(int x, int y, unsigned record, bool whole) {
    unsigned char* const rec = move_script::At(::bof3::Img(CharacterRecords_addr) + record * 0xA4u);
    if (whole) SH_CALL(Menu_DrawMemberStatus)(x, y, record, 0);
    SH_CALL(Menu_DrawPieces)(x, y, move_script::At(::bof3::Img(Menu_MemberPieces_addr)), 2);
    SH_CALL(Menu_DrawExpBar)(x + 0x14, y + 0x12, record, rec[0xA], static_cast<unsigned>(rv::As<records::CharRecord>(rec)->exp));
    if (kChampionClearEmblem) SH_CALL(Menu_DrawPiece)(x + 0x2F, y + 0x15, 0x3D, 0);
}

void Fill(Reserve& r, const unsigned char* party, unsigned party_count) {
    r.count = 0;
    r.top = 0;
    for (unsigned k = 0; k < 8; ++k) {
        if ((CharacterRecords[k].flags & 1) == 0) continue;
        const unsigned char id = CharacterRecords[k].char_id;
        unsigned j = 0;
        while (j < party_count && party[j] != id) ++j;
        if (j == party_count) r.ids[r.count++] = id;
    }
}

unsigned Shown(const Reserve& r) {
    const unsigned left = r.count > r.top ? static_cast<unsigned>(r.count - r.top) : 0u;
    return left < kRows ? left : kRows;
}

void Window(const Reserve& r, unsigned char* list) {
    const unsigned n = Shown(r);
    for (unsigned i = 0; i < n; ++i) list[i] = r.ids[r.top + i];
}

int Whole(const Reserve& r, int shown) { return r.top + shown; }

int ShownRow(const Reserve& r, int whole) {
    const int s = whole - r.top;
    return s >= 0 && s < static_cast<int>(Shown(r)) ? s : -1;
}

namespace {

// top moved the least that shows `whole`; then kept so the rows shown are as
// many as the list allows (no empty row below while rows are above).
void Follow(Reserve& r, int whole) {
    if (whole < r.top) r.top = static_cast<unsigned char>(whole);
    if (whole >= r.top + static_cast<int>(kRows)) r.top = static_cast<unsigned char>(whole - static_cast<int>(kRows) + 1);
    const int last_top = r.count > kRows ? r.count - static_cast<int>(kRows) : 0;
    if (r.top > last_top) r.top = static_cast<unsigned char>(last_top);
}

}  // namespace

int Hold(Reserve& r, int whole) {
    if (r.count == 0) {
        r.top = 0;
        return 0;
    }
    if (whole > r.count - 1) whole = r.count - 1;
    if (whole < 0) whole = 0;
    Follow(r, whole);
    return whole;
}

int Move(Reserve& r, int whole, int step) {
    if (r.count == 0) {
        r.top = 0;
        return 0;
    }
    whole += step;
    if (whole < 0) whole = r.count - 1;
    if (whole > r.count - 1) whole = 0;
    Follow(r, whole);
    return whole;
}

void SelfTest() {
    int checks = 0;
    auto make = [](unsigned count) {
        Reserve r;
        r.count = static_cast<unsigned char>(count);
        for (unsigned i = 0; i < count; ++i) r.ids[i] = static_cast<unsigned char>(0x10 + i);
        return r;
    };
    // Every list length 0..8: walking down from row 0 visits every row once,
    // in order, each shown, and wraps to 0; walking up does the same backwards.
    for (unsigned n = 0; n <= kMaxReserve; ++n) {
        Reserve r = make(n);
        int row = Hold(r, 0);
        for (unsigned step = 0; step < 2 * n + 1; ++step) {
            const int want = n == 0 ? 0 : static_cast<int>((step + 1) % n);
            row = Move(r, row, +1);
            if (row != want) bof3::Fatal("solo_screens self-test: %u rows, step %u down gives row %d, not %d", n, step, row, want);
            if (n != 0 && ShownRow(r, row) < 0) bof3::Fatal("solo_screens self-test: %u rows, row %d not shown (top %u)", n, row, r.top);
            if (Shown(r) != (n < kRows ? n : kRows)) bof3::Fatal("solo_screens self-test: %u rows, %u shown at top %u", n, Shown(r), r.top);
            ++checks;
        }
        row = Hold(r, 0);
        for (unsigned step = 0; step < 2 * n + 1; ++step) {
            const int want = n == 0 ? 0 : static_cast<int>((2 * n - 1 - step % n) % n);
            row = Move(r, row, -1);
            if (row != want) bof3::Fatal("solo_screens self-test: %u rows, step %u up gives row %d, not %d", n, step, row, want);
            if (n != 0 && ShownRow(r, row) < 0) bof3::Fatal("solo_screens self-test: %u rows, row %d not shown (top %u)", n, row, r.top);
            ++checks;
        }
        // The window holds the rows shown, in order.
        unsigned char list[kRows] = {0xEE, 0xEE, 0xEE};
        Window(r, list);
        for (unsigned i = 0; i < kRows; ++i) {
            const unsigned char want = i < Shown(r) ? static_cast<unsigned char>(0x10 + r.top + i) : 0xEE;
            if (list[i] != want) bof3::Fatal("solo_screens self-test: %u rows, window byte %u is 0x%X", n, i, list[i]);
        }
        ++checks;
    }
    // Three rows or fewer never scroll: the original's screen, row for row.
    for (unsigned n = 1; n <= kRows; ++n) {
        Reserve r = make(n);
        int row = 0;
        for (unsigned step = 0; step < 7; ++step) {
            row = Move(r, row, step & 1 ? -1 : +1);
            if (r.top != 0 || ShownRow(r, row) != row) bof3::Fatal("solo_screens self-test: %u rows scrolled", n);
        }
        ++checks;
    }
    // Hold: a row past the last is held at it; four rows from a shown row 2.
    {
        Reserve r = make(4);
        if (Hold(r, 6) != 3 || r.top != 1) bof3::Fatal("solo_screens self-test: hold past the last");
        if (Hold(r, Whole(r, 0)) != 1 || r.top != 1) bof3::Fatal("solo_screens self-test: hold of a shown row");
        if (Move(r, 1, -1) != 0 || r.top != 0) bof3::Fatal("solo_screens self-test: scroll up");
        if (Move(r, 0, -1) != 3 || r.top != 1 || ShownRow(r, 3) != 2) bof3::Fatal("solo_screens self-test: wrap up");
        if (Move(r, 3, +1) != 0 || r.top != 0) bof3::Fatal("solo_screens self-test: wrap down");
        checks += 5;
    }
    bof3::Log("solo_screens self-test: %d checks passed", checks);
}

}  // namespace solo_screens
