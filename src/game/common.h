// Rufe.org LLC 2022-2025: ISC License
#ifndef COMMON
#define COMMON

#ifndef __APPLE__
enum { __APPLE__ };
#endif

#ifndef ANDROID
enum { ANDROID };
#endif

#ifndef RELEASE
enum { RELEASE };
#endif

// stdint
typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;

typedef int (*fn)();

#ifndef UINT64_MAX
#define UINT64_MAX __UINT64_MAX__
#define UINT32_MAX __UINT32_MAX__
#define UINT16_MAX __UINT16_MAX__
#define UINT8_MAX __UINT8_MAX__
#define INT64_MAX __INT64_MAX__
#define INT32_MAX __INT32_MAX__
#define INT16_MAX __INT16_MAX__
#define INT8_MAX __INT8_MAX__
#endif

#ifndef INT64_MIN
#define INT64_MIN ((int64_t)~0x7fffffffffffffff)
#define INT32_MIN ((int32_t)~0x7fffffff)
#define INT16_MIN ((int16_t)~0x7fff)
#define INT8_MIN ((int8_t)~0x7f)
#endif

// stdbool
#define BOOL int

// Default Type init
#define DFT(x) ((x){0})

// Array Clear
#define AC(x) memset(x, 0, sizeof(x))
// Array Length
#define AL(x) (sizeof(x) / sizeof(x[0]))
// Array Pair
#define AP(x) x, AL(x)
// Array Modulus
#define AM(arr, id) ((id) % AL(arr))
// Array Slot
#define AS(arr, id) arr[(id) % AL(arr)]
// Array End
#define AE(arr) ((uint8_t*)arr + sizeof(arr))
// String Length
#define SL(str) (AL(str) - 1)
// String Pair
#define S2(str) str, SL(str)

#define ABS(x) ((x) > 0 ? (x) : -(x))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define ST_MAX(wv, rv) \
  if (rv > wv) wv = rv
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define ST_MIN(wv, rv) \
  if (rv < wv) wv = rv
#define XOR(x, y) ((uint64_t)(x) ^ (uint64_t)(y))
#define INVERT(x) x = !x
#define OF2(x) (((x - 1) & x) == 0)
#define LOGFMT(...)
//#define LOGFMT(x, ...) printf(x "\n", ##__VA_ARGS__)

// Named noop that can be overridden
#define WEAK_NOOP(x) \
  int x() __attribute__((weak)) __attribute__((alias("noop")))

// Use global var
#define USE(x) typeof(x##D) x = x##D
// Array Use
#define AUSE(a, idx) typeof(a##D[0]) a = a##D[idx];
// Member Use
#define MUSE(a, member) typeof(a##D.member) member = a##D.member

// global declaration
#ifndef GLOBAL
#define GLOBAL
#endif
// (optionally defined)
#ifndef DATA
#define DATA static
#endif
#define STATIC static

// game data section for determinism verification
#if defined(__FATCOSMOCC__)
// ape.lds: SECTION prefix based cosmocc defaults
#define SECTION ".sort.data"
__attribute__((aligned(64)))
__attribute__((section(SECTION ".game.alpha"))) unsigned char __start_game[0];
#define GAME __attribute__((section(SECTION ".game.play")))
__attribute__((aligned(64)))
__attribute__((section(SECTION ".game.zed"))) unsigned char __stop_game[0];

#elif defined(__APPLE__)
#define GAME DATA __attribute__((section("__DATA,game")))
extern unsigned char __start_game[] __asm("section$start$__DATA$game");
extern unsigned char __stop_game[] __asm("section$end$__DATA$game");

#else
#define GAME DATA __attribute__((section("game")))
extern unsigned char __start_game[] __attribute__((__weak__));
extern unsigned char __stop_game[] __attribute__((__weak__));
#endif

#ifndef CTRL
#define CTRL(x) (x & 0x1f)
#endif

// pool of structs; see codegen.sh
#define POOL
#define FOR_EACH(type, body)                                     \
  {                                                              \
    for (int it_index = 0; it_index < AL(type##D); ++it_index) { \
      struct type##S* type = &entity_##type##D[it_index];        \
      if (!type->id) continue;                                   \
      body;                                                      \
    }                                                            \
  }

// Function Table
#define FT(x) ftable_clear(&x##D, sizeof(x##D) / sizeof(fn))
int
noop()
{
  return 0;
}
STATIC int
ftable_clear(void* ftable, int size)
{
  fn* func = ftable;
  for (int it = 0; it < size; ++it) func[it] = noop;
  return 0;
}

// Vector
typedef int* vectorT;
typedef int v2[2];
typedef int v3[3];
typedef int v4[4];
#define V2(v) v[0], v[1]
#define V3(v) v[0], v[1], v[2]
#define V4(v) v[0], v[1], v[2], v[3]
#define V2c(v) V2(cptr(v))
#define V3c(v) V3(cptr(v))
#define V4c(v) V4(cptr(v))
#define V2b(v) V2(bptr(v))
#define V3b(v) V3(bptr(v))
#define V4b(v) V4(bptr(v))
#define V2i(v) V2(iptr(v))
#define V3i(v) V3(iptr(v))
#define V4i(v) V4(iptr(v))
#define V2f(v) V2(fptr(v))
#define V3f(v) V3(fptr(v))
#define V4f(v) V4(fptr(v))
#define XY(v) (v).x, (v).y
#define WH(r) (r).w, (r).h

#endif
