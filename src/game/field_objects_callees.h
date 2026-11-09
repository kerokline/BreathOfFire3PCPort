// The callees seam of field_objects.cpp: the struct and the one `g` its functions call through, so that a
// function moved out of field_objects.cpp calls through the same pointers the start-up fuzz swaps
// (source-layout B0, analysis/tu_renames.tsv). Defined in field_objects.cpp.
#pragma once

#include <cstdint>

namespace field_objects {

// Every callee, through pointers so that the start-up fuzz can stand recording
// functions in for them - for the original's copies and for ours alike. Two
// of them are ours (the motion and the settle below); the fuzz tests each of
// the four alone.
struct CalleesFieldObjects {
    unsigned char (__cdecl* step)(unsigned char*, const unsigned char*);
    void (__cdecl* pose_wait)(unsigned char*);
    unsigned char (__cdecl* blocked)(unsigned char*);
    void (__cdecl* move)(unsigned char*, unsigned char);
    void (__cdecl* settle)(unsigned char*, unsigned char);
    void (__cdecl* motion)(unsigned char*);
    long (__cdecl* elevation)(long, long);
    void (__cdecl* party_apply)(unsigned char*);
    void (__cdecl* party_move)(unsigned char*, unsigned char);
    unsigned char (__cdecl* by_handle)(unsigned char);
    void (__cdecl* attach_offset)(long*, unsigned char);
    void (__cdecl* update)(unsigned char*);
    void (__cdecl* push_matrix)();
    short* (__cdecl* rot_matrix)(const short*, short*);
    void (__cdecl* trans_matrix)(unsigned long*, const unsigned long*);
    void (__cdecl* set_rot)(const unsigned long*);
    void (__cdecl* set_trans)(const unsigned long*);
    void (__cdecl* rot_trans)(const short*, long*);
    void (__cdecl* pop_matrix)();
};

extern CalleesFieldObjects g;   // defined in field_objects.cpp (source-layout B0: promoted for a moved reader)
}  // namespace field_objects
