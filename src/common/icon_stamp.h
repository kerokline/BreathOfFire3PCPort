// bof3x.icon: the record that the executable's icon was generated
// (docs/exe-icon.md, DIV-0095). The engine writes it beside bof3x.exe when it
// has written bof3x.exe.new; the launcher reads it before it moves that file
// into place. Three SHA-256s, lowercase hex:
//
//   sheet  the bytes the picture was cut from (the icon's record, its sixteen
//          CLUT halfwords and its 40 x 48 texture region in the VRAM shadow)
//   from   bof3x.exe as it was when the engine copied it
//   to     bof3x.exe.new as the engine wrote and verified it
//
// The launcher moves the new file in only when bof3x.exe is still `from` and
// the new file is `to`: a rebuilt bof3x.exe is never replaced by a copy of
// the one before it.
#pragma once

#include <string>

namespace bof3x {

struct IconStamp {
    std::string sheet, from, to;
};

// False when the file is missing or not a stamp of this form.
bool IconStampRead(const std::wstring& path, IconStamp& stamp);

// Written to `path`.tmp and moved over `path`. False, with `why`, on failure.
bool IconStampWrite(const std::wstring& path, const IconStamp& stamp, std::string& why);

}  // namespace bof3x
