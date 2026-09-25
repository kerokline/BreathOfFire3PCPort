// A synthetic corpus for the x86 -> C lifter prototype (docs/lifter-feasibility.md).
//
// No game code and no game data: every function here was written for this file.
// Each one imitates a SHAPE the docs record in BOF3.exe - fixed-address record
// arrays, switch jump tables, handler tables called through pointers, x87
// arithmetic under a 53-bit control word, 64-bit fixed-point products, byte
// tables, stdcall and fastcall - so the lifter meets those shapes on code we
// may publish. build.sh compiles it to a /FIXED i686 PE at 0x400000 with no
// CRT, the way BOF3.exe is laid out.
//
// Every function is total: any argument and any contents of the globals give
// a defined result, so the fuzz can feed it random bytes. Indices are masked,
// divisors are forced non-zero, and float inputs come from integers.
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef short s16;
typedef unsigned int u32;
typedef int s32;
typedef long long s64;
typedef unsigned long long u64;

void* memcpy(void* d, const void* s, u32 n) {
    u8* dd = (u8*)d;
    const u8* ss = (const u8*)s;
    while (n--) *dd++ = *ss++;
    return d;
}
void* memset(void* d, int c, u32 n) {
    u8* dd = (u8*)d;
    while (n--) *dd++ = (u8)c;
    return d;
}

// --- records at fixed addresses (the sprite object arrays' shape) ----------
typedef struct {
    s16 x, y, z;
    u16 flags;
    u8 kind, state, timer, pad;
    s32 vx, vy;
    u8 name[12];
} Obj;

Obj g_objs[30];
s32 g_counter;
u8 g_table[256];
s16 g_sine[64];
s32 g_matrix[9];
s32 g_vec[3];
s32 g_out[3];
double g_scale;
float g_fx[8];
u32 g_log[16];
u32 g_log_n;

// Pure integer arithmetic, signed and unsigned widths.
s32 Arith_Mix(s32 a, s32 b) {
    s32 r = a * 3 - (b >> 2);
    r ^= (u32)b >> 5;
    if (r < 0) r = -r;
    return r + (s8)a + (u16)b;
}

// Signed ordering without masking: the compares whose overflow flag decides.
s32 Signed_Order(s32 a, s32 b) {
    s32 r = 0;
    if (a < b) r |= 1;
    if (a > b) r |= 2;
    if (a <= (b ^ 0x40000000)) r |= 4;
    if (a - 5 >= b) r |= 8;
    return r;
}

// Division: signed and unsigned, divisor never zero, INT_MIN / -1 avoided.
s32 Arith_Div(s32 a, s32 b) {
    s32 d = (b & 0xFF) + 1;
    u32 q = (u32)a / (u32)d;
    s32 m = (a | 1) % d;
    return (s32)q + m * 7;
}

// A 64-bit fixed-point product, the GTE's shape: (a * b) >> 12.
s32 Fixed_Mul(s32 a, s32 b) {
    return (s32)(((s64)a * b) >> 12);
}

// 64-bit unsigned product, both halves returned in edx:eax.
u64 Wide_Mul(u32 a, u32 b) {
    return (u64)a * b + (a >> 3);
}

// A matrix times a vector with 64-bit accumulation: ApplyMatrix's shape.
void Gte_Apply(void) {
    for (int i = 0; i < 3; ++i) {
        s64 acc = 0;
        for (int j = 0; j < 3; ++j) acc += (s64)g_matrix[i * 3 + j] * g_vec[j];
        g_out[i] = (s32)(acc >> 12);
    }
}

// Walk the record array, update each live object.
s32 Obj_UpdateAll(void) {
    s32 live = 0;
    for (int i = 0; i < 30; ++i) {
        Obj* o = &g_objs[i];
        if (!(o->flags & 1)) continue;
        o->x += (s16)(o->vx >> 8);
        o->y += (s16)(o->vy >> 8);
        if (o->timer) --o->timer;
        else o->state = (u8)(o->state + 1) & 7;
        ++live;
    }
    g_counter += live;
    return live;
}

// Find the nearest live object to a point: Sprite_FindNearby's shape.
s32 Obj_FindNearest(s32 x, s32 y) {
    s32 best = -1;
    u32 best_d = 0xFFFFFFFFu;
    x = (s16)x;
    y = (s16)y;
    for (int i = 0; i < 30; ++i) {
        if (!(g_objs[i].flags & 1)) continue;
        s32 dx = g_objs[i].x - x, dy = g_objs[i].y - y;
        u32 d = (u32)(dx * dx) + (u32)(dy * dy);
        if (d < best_d) best_d = d, best = i;
    }
    return best;
}

// A dense switch: compiles to a jump table in .text or .rdata.
s32 Script_Op(u32 op, s32 a, s32 b) {
    switch (op & 15) {
    case 0: return a + b;
    case 1: return a - b;
    case 2: return a & b;
    case 3: return a | b;
    case 4: return a ^ b;
    case 5: return a << (b & 31);
    case 6: return a >> (b & 31);
    case 7: return (u32)a >> (b & 31);
    case 8: return a == b;
    case 9: return a < b;
    case 10: return (u32)a < (u32)b;
    case 11: g_counter = a; return b;
    case 12: return g_table[a & 0xFF];
    default: return -1;
    }
}

// A switch whose table is indexed through a byte table first (the
// Gfx_DrawOTag shape: jump table plus byte index table).
s32 Script_Sparse(u32 op) {
    switch (op & 0xFF) {
    case 0x00: return 10;
    case 0x03: return 11;
    case 0x07: return 12;
    case 0x10: return 13;
    case 0x11: return 14;
    case 0x20: return 15;
    case 0x40: return 16;
    case 0x41: return 17;
    case 0x80: return 18;
    case 0xFE: return 19;
    default: return 0;
    }
}

// Handlers reached only through a pointer table: the battle state handlers'
// shape. pe_funcs.py-style scanning cannot see these from call sites.
typedef s32 (*Handler)(Obj*);
static s32 H_Idle(Obj* o) { return o->kind; }
static s32 H_Move(Obj* o) { o->x = (s16)(o->x + 1); return 1; }
static s32 H_Wait(Obj* o) { return o->timer ? --o->timer : 2; }
static s32 H_Die(Obj* o) { o->flags = 0; return 3; }
Handler const g_handlers[4] = {H_Idle, H_Move, H_Wait, H_Die};

s32 Obj_Dispatch(u32 i) {
    Obj* o = &g_objs[i % 30];
    return g_handlers[o->state & 3](o);
}

// stdcall and fastcall: the callee cleans the stack / args in registers.
s32 __attribute__((stdcall)) Std_Sum3(s32 a, s32 b, s32 c) {
    return a + b * 2 + c * 3;
}
s32 __attribute__((fastcall)) Fast_Sel(s32 a, s32 b, s32 c) {
    return (a & 1) ? b : c;
}
s32 Call_Conventions(s32 a, s32 b) {
    return Std_Sum3(a, b, a ^ b) + Fast_Sel(a, b, a - b);
}

// x87: doubles, a float store's rounding, and float -> int conversions
// (the _ftol shape: truncation under a changed rounding mode).
s32 Fpu_Lerp(s32 a, s32 b, s32 t) {
    double fa = a, fb = b, ft = (double)(t & 0xFFFF) / 65536.0;
    double r = fa + (fb - fa) * ft;
    return (s32)r;
}

s32 Fpu_Scale(s32 i) {
    float f = (float)(i >> 4) * 0.1f;   // rounds to float here
    g_fx[i & 7] = f;
    double d = (double)f * g_scale;
    if (!(d < 2147483647.0 && d > -2147483648.0)) return 0;
    return (s32)d;
}

// A float return value: in st(0).
double Fpu_Dist(s32 x, s32 y) {
    double dx = (s16)x, dy = (s16)y;
    double s = dx * dx + dy * dy;
    double r = s;
    for (int k = 0; k < 6; ++k) r = 0.5 * (r + s / (r + 1.0));   // no libm sqrt
    return r;
}

s32 Fpu_Compare(s32 a, s32 b) {
    double x = a * 0.5, y = b * 0.25;
    if (x < y) return 1;
    if (x > y) return 2;
    return 3;
}

// Byte tables and sign extension: the text path's shape.
s32 Text_Width(u32 start, u32 n) {
    s32 w = 0;
    n &= 31;
    for (u32 i = 0; i < n; ++i) {
        u8 c = g_table[(start + i) & 0xFF];
        if (c == 0) break;
        if (c >= 0x80) { w += (s8)g_table[(c * 3) & 0xFF] & 15; continue; }
        w += (c & 7) + 4;
    }
    return w;
}

// memcpy / memset calls and a struct copy.
void Obj_Copy(u32 to, u32 from) {
    Obj tmp = g_objs[from % 30];
    tmp.flags |= 0x8000;
    g_objs[to % 30] = tmp;
    memset(g_objs[(to + 1) % 30].name, 0x20, 12);
}

// Recursion.
u32 Rec_Sum(u32 n) {
    n &= 63;
    return n ? n + Rec_Sum(n - 1) : 0;
}

// A sine table lookup with interpolation: the GTE's rsin shape.
s32 Sine_At(u32 angle) {
    u32 i = (angle >> 6) & 63, f = angle & 63;
    s32 a = g_sine[i], b = g_sine[(i + 1) & 63];
    return a + (((b - a) * (s32)f) >> 6);
}

// Bit rotation and shift-heavy hashing.
u32 Hash_Bytes(u32 seed, u32 n) {
    u32 h = seed ^ 0x9E3779B9u;
    n &= 63;
    for (u32 i = 0; i < n; ++i) {
        h ^= g_table[i];
        h = (h << 5) | (h >> 27);
        h *= 0x01000193u;
    }
    return h;
}

// A 64-bit shift by a variable count (shld / shrd and the >= 32 branch).
u64 Wide_Shift(u32 lo, u32 hi, u32 n) {
    u64 v = ((u64)hi << 32) | lo;
    n &= 63;
    return (v << n) ^ (v >> (63 - n));
}

// A log buffer: a global index and a store, bounds-masked.
void Log_Push(u32 v) {
    g_log[g_log_n & 15] = v;
    g_log_n = (g_log_n + 1) & 0xFF;
}

// Calls a pointer-reached function directly and through a table, then logs.
s32 Frame_Step(u32 i) {
    s32 r = Obj_Dispatch(i) + Obj_UpdateAll();
    Log_Push((u32)r);
    return r;
}

void entry(void) {}
