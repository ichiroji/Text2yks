//---------------------------------------------------------------------------//
//  inttypes.h（VS2008 専用の代用品）
//      VS2008 には C99 の <stdint.h>・<inttypes.h> が無いが、PCRE2 は <inttypes.h> を必須とする。
//      PCRE2 の NON-AUTOTOOLS-BUILD の指示どおり、PCRE2 のソースは変えずにこのヘッダーで型を補う。
//      VS2008 のプロジェクト（.vcproj）の include パスにだけ入れる。VS2010 以降は標準のものを使う。
//      このファイルは Text2yks の一部（MIT ライセンス）。
//---------------------------------------------------------------------------//
#ifndef TEXT2YKS_COMPAT_VS2008_INTTYPES_H
#define TEXT2YKS_COMPAT_VS2008_INTTYPES_H

#if !defined(_MSC_VER) || (_MSC_VER >= 1600)
#error "This inttypes.h is only for Visual Studio 2008 (_MSC_VER < 1600)."
#endif

#include <limits.h>
#include <stddef.h>

// 幅指定の整数型
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef __int64            int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned __int64   uint64_t;

// 最小幅の整数型
typedef int8_t             int_least8_t;
typedef int16_t            int_least16_t;
typedef int32_t            int_least32_t;
typedef int64_t            int_least64_t;
typedef uint8_t            uint_least8_t;
typedef uint16_t           uint_least16_t;
typedef uint32_t           uint_least32_t;
typedef uint64_t           uint_least64_t;

// 高速な整数型
typedef int8_t             int_fast8_t;
typedef int32_t            int_fast16_t;
typedef int32_t            int_fast32_t;
typedef int64_t            int_fast64_t;
typedef uint8_t            uint_fast8_t;
typedef uint32_t           uint_fast16_t;
typedef uint32_t           uint_fast32_t;
typedef uint64_t           uint_fast64_t;

// 最大幅の整数型（intptr_t・uintptr_t は VS2008 の <stddef.h> にある）
typedef int64_t            intmax_t;
typedef uint64_t           uintmax_t;

// 上限・下限（C++ では __STDC_LIMIT_MACROS が無くても定義する）
#define INT8_MIN           (-127 - 1)
#define INT16_MIN          (-32767 - 1)
#define INT32_MIN          (-2147483647 - 1)
#define INT64_MIN          (-9223372036854775807i64 - 1)
#define INT8_MAX           127
#define INT16_MAX          32767
#define INT32_MAX          2147483647
#define INT64_MAX          9223372036854775807i64
#define UINT8_MAX          0xffU
#define UINT16_MAX         0xffffU
#define UINT32_MAX         0xffffffffU
#define UINT64_MAX         0xffffffffffffffffui64

#define INTMAX_MIN         INT64_MIN
#define INTMAX_MAX         INT64_MAX
#define UINTMAX_MAX        UINT64_MAX

#ifndef SIZE_MAX
#ifdef _WIN64
#define SIZE_MAX           UINT64_MAX
#else
#define SIZE_MAX           UINT32_MAX
#endif
#endif

#ifdef _WIN64
#define INTPTR_MIN         INT64_MIN
#define INTPTR_MAX         INT64_MAX
#define UINTPTR_MAX        UINT64_MAX
#define PTRDIFF_MIN        INT64_MIN
#define PTRDIFF_MAX        INT64_MAX
#else
#define INTPTR_MIN         INT32_MIN
#define INTPTR_MAX         INT32_MAX
#define UINTPTR_MAX        UINT32_MAX
#define PTRDIFF_MIN        INT32_MIN
#define PTRDIFF_MAX        INT32_MAX
#endif

// 定数のマクロ
#define INT8_C(v)          (v)
#define INT16_C(v)         (v)
#define INT32_C(v)         (v)
#define INT64_C(v)         (v##i64)
#define UINT8_C(v)         (v##U)
#define UINT16_C(v)        (v##U)
#define UINT32_C(v)        (v##U)
#define UINT64_C(v)        (v##ui64)
#define INTMAX_C(v)        INT64_C(v)
#define UINTMAX_C(v)       UINT64_C(v)

// printf 用の書式（PCRE2 のライブラリ本体では使わないが、最低限を用意する）
#define PRId32             "d"
#define PRIi32             "i"
#define PRIu32             "u"
#define PRIx32             "x"
#define PRIX32             "X"
#define PRId64             "I64d"
#define PRIi64             "I64i"
#define PRIu64             "I64u"
#define PRIx64             "I64x"
#define PRIX64             "I64X"

#endif // TEXT2YKS_COMPAT_VS2008_INTTYPES_H
