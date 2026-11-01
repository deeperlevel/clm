#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include <stdint.h>
typedef uint64_t clm_id;

#define BYTE_MIN 0
#define BYTE_MAX 255

#define SBYTE_MIN -128
#define SBYTE_MAX 127

#define SHORT_MIN -32768
#define SHORT_MAX 32767

#define USHORT_MIN 0
#define USHORT_MAX 65535

#undef INT_MIN
#undef INT_MAX
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647

#define UINT_MIN 0
#undef UINT_MAX
#define UINT_MAX 4294967295U

#undef LONG_MIN
#undef LONG_MAX
#define LONG_MIN (-9223372036854775807LL - 1)
#define LONG_MAX 9223372036854775807LL

#define ULONG_MIN 0
#undef ULONG_MAX
#define ULONG_MAX 18446744073709551615ULL

#define FLOAT_MIN -3.402823466e+38F
#define FLOAT_MAX 3.402823466e+38F

#define DOUBLE_MIN -1.7976931348623157e+308
#define DOUBLE_MAX 1.7976931348623157e+308

typedef int8_t   clm_sbyte;
typedef uint8_t  clm_byte;
typedef int16_t  clm_short;
typedef uint16_t clm_ushort;
typedef int32_t  clm_int;
typedef uint32_t clm_uint;
typedef int64_t  clm_long;
typedef uint64_t clm_ulong;
typedef float    clm_float;
typedef double   clm_double;

// =============================================================================
// clm_position
// =============================================================================
typedef struct {
    clm_sbyte x;
    clm_sbyte y;
} clm_position_i8;
typedef clm_position_i8 clm_position_sbyte;

typedef struct {
    clm_byte x;
    clm_byte y;
} clm_position_u8;
typedef clm_position_i8 clm_position_byte;

typedef struct {
    clm_short x;
    clm_short y;
} clm_position_i16;
typedef clm_position_i16 clm_position_short;

typedef struct {
    clm_ushort x;
    clm_ushort y;
} clm_position_u16;
typedef clm_position_u16 clm_position_ushort;

typedef struct {
    clm_int x;
    clm_int y;
} clm_position_i32;
typedef clm_position_i32 clm_position;
typedef clm_position_i32 clm_position_int;

typedef struct {
    clm_uint x;
    clm_uint y;
} clm_position_u32;
typedef clm_position_u32 clm_position_uint;

typedef struct {
    clm_long x;
    clm_long y;
} clm_position_i64;
typedef clm_position_i64 clm_position_long;

typedef struct {
    clm_ulong x;
    clm_ulong y;
} clm_position_u64;
typedef clm_position_u64 clm_position_ulong;

typedef struct {
    clm_float x;
    clm_float y;
} clm_position_f32;
typedef clm_position_f32 clm_position_float;

typedef struct {
    clm_double x;
    clm_double y;
} clm_position_f64;
typedef clm_position_f64 clm_position_double;

// =============================================================================
// clm_position_3d
// =============================================================================
typedef struct {
    clm_sbyte x;
    clm_sbyte y;
    clm_sbyte z;
} clm_position_3d_i8;
typedef clm_position_3d_i8 clm_position_3d_sbyte;

typedef struct {
    clm_byte x;
    clm_byte y;
    clm_byte z;
} clm_position_3d_u8;
typedef clm_position_3d_u8 clm_position_3d_byte;

typedef struct {
    clm_short x;
    clm_short y;
    clm_short z;
} clm_position_3d_i16;
typedef clm_position_3d_i16 clm_position_3d_short;

typedef struct {
    clm_ushort x;
    clm_ushort y;
    clm_ushort z;
} clm_position_3d_u16;
typedef clm_position_3d_u16 clm_position_3d_ushort;

typedef struct {
    clm_int x;
    clm_int y;
    clm_int z;
} clm_position_3d_i32;
typedef clm_position_3d_i32 clm_position_3d;
typedef clm_position_3d_i32 clm_position_3d_int;

typedef struct {
    clm_uint x;
    clm_uint y;
    clm_uint z;
} clm_position_3d_u32;
typedef clm_position_3d_u32 clm_position_3d_uint;

typedef struct {
    clm_long x;
    clm_long y;
    clm_long z;
} clm_position_3d_i64;
typedef clm_position_3d_i64 clm_position_3d_long;

typedef struct {
    clm_ulong x;
    clm_ulong y;
    clm_ulong z;
} clm_position_3d_u64;
typedef clm_position_3d_u64 clm_position_3d_ulong;

typedef struct {
    clm_float x;
    clm_float y;
    clm_float z;
} clm_position_3d_f32;
typedef clm_position_3d_f32 clm_position_3d_float;

typedef struct {
    clm_double x;
    clm_double y;
    clm_double z;
} clm_position_3d_f64;
typedef clm_position_3d_f64 clm_position_3d_double;


// =============================================================================
// clm_size
// =============================================================================
typedef struct {
    clm_sbyte width;
    clm_sbyte height;
} clm_size_i8;
typedef clm_size_i8 clm_size_sbyte;

typedef struct {
    clm_byte width;
    clm_byte height;
} clm_size_u8;
typedef clm_size_u8 clm_size_byte;

typedef struct {
    clm_short width;
    clm_short height;
} clm_size_i16;
typedef clm_size_i16 clm_size_short;

typedef struct {
    clm_ushort width;
    clm_ushort height;
} clm_size_u16;
typedef clm_size_u16 clm_size_ushort;

typedef struct {
    clm_int width;
    clm_int height;
} clm_size_i32;
typedef clm_size_i32 clm_size;
typedef clm_size_i32 clm_size_int;

typedef struct {
    clm_uint width;
    clm_uint height;
} clm_size_u32;
typedef clm_size_u32 clm_size_uint;

typedef struct {
    clm_long width;
    clm_long height;
} clm_size_i64;
typedef clm_size_i64 clm_size_long;

typedef struct {
    clm_ulong width;
    clm_ulong height;
} clm_size_u64;
typedef clm_size_u64 clm_size_ulong;

typedef struct {
    clm_float width;
    clm_float height;
} clm_size_f32;
typedef clm_size_f32 clm_size_float;

typedef struct {
    clm_double width;
    clm_double height;
} clm_size_f64;
typedef clm_size_f64 clm_size_double;

// =============================================================================
// clm_size_3d
// =============================================================================
typedef struct {
    clm_sbyte width;
    clm_sbyte height;
    clm_sbyte depth;
} clm_size_3d_i8;
typedef clm_size_3d_i8 clm_size_3d_sbyte;

typedef struct {
    clm_byte width;
    clm_byte height;
    clm_byte depth;
} clm_size_3d_u8;
typedef clm_size_3d_u8 clm_size_3d_byte;

typedef struct {
    clm_short width;
    clm_short height;
    clm_short depth;
} clm_size_3d_i16;
typedef clm_size_3d_i16 clm_size_3d_short;

typedef struct {
    clm_ushort width;
    clm_ushort height;
    clm_ushort depth;
} clm_size_3d_u16;
typedef clm_size_3d_u16 clm_size_3d_ushort;

typedef struct {
    clm_int width;
    clm_int height;
    clm_int depth;
} clm_size_3d_i32;
typedef clm_size_3d_i32 clm_size_3d;
typedef clm_size_3d_i32 clm_size_3d_int;

typedef struct {
    clm_uint width;
    clm_uint height;
    clm_uint depth;
} clm_size_3d_u32;
typedef clm_size_3d_u32 clm_size_3d_uint;

typedef struct {
    clm_long width;
    clm_long height;
    clm_long depth;
} clm_size_3d_i64;
typedef clm_size_3d_i64 clm_size_3d_long;

typedef struct {
    clm_ulong width;
    clm_ulong height;
    clm_ulong depth;
} clm_size_3d_u64;
typedef clm_size_3d_u64 clm_size_3d_ulong;

typedef struct {
    clm_float width;
    clm_float height;
    clm_float depth;
} clm_size_3d_f32;
typedef clm_size_3d_f32 clm_size_3d_float;

typedef struct {
    clm_double width;
    clm_double height;
    clm_double depth;
} clm_size_3d_f64;
typedef clm_size_3d_f64 clm_size_3d_double;

// =============================================================================
// clm_block
// =============================================================================
typedef struct {
    clm_sbyte x;
    clm_sbyte y;
    clm_sbyte width;
    clm_sbyte height;
} clm_block_i8;
typedef clm_block_i8 clm_block_sbyte;

typedef struct {
    clm_byte x;
    clm_byte y;
    clm_byte width;
    clm_byte height;
} clm_block_u8;
typedef clm_block_u8 clm_block_byte;

typedef struct {
    clm_short x;
    clm_short y;
    clm_short width;
    clm_short height;
} clm_block_i16;
typedef clm_block_i16 clm_block_short;

typedef struct {
    clm_ushort x;
    clm_ushort y;
    clm_ushort width;
    clm_ushort height;
} clm_block_u16;
typedef clm_block_u16 clm_block_ushort;

typedef struct {
    clm_int x;
    clm_int y;
    clm_int width;
    clm_int height;
} clm_block_i32;
typedef clm_block_i32 clm_block; // Default baseline test target
typedef clm_block_i32 clm_block_int;

typedef struct {
    clm_uint x;
    clm_uint y;
    clm_uint width;
    clm_uint height;
} clm_block_u32;
typedef clm_block_u32 clm_block_uint;

typedef struct {
    clm_long x;
    clm_long y;
    clm_long width;
    clm_long height;
} clm_block_i64;
typedef clm_block_i64 clm_block_long;

typedef struct {
    clm_ulong x;
    clm_ulong y;
    clm_ulong width;
    clm_ulong height;
} clm_block_u64;
typedef clm_block_u64 clm_block_ulong;

typedef struct {
    clm_float x;
    clm_float y;
    clm_float width;
    clm_float height;
} clm_block_f32;
typedef clm_block_f32 clm_block_float;

typedef struct {
    clm_double x;
    clm_double y;
    clm_double width;
    clm_double height;
} clm_block_f64;
typedef clm_block_f64 clm_block_double;


// =============================================================================
// clm_block_3d
// =============================================================================
typedef struct {
    clm_sbyte x;
    clm_sbyte y;
    clm_sbyte z;
    clm_sbyte width;
    clm_sbyte height;
    clm_sbyte depth;
} clm_block_3d_i8;
typedef clm_block_3d_i8 clm_block_3d_sbyte;

typedef struct {
    clm_byte x;
    clm_byte y;
    clm_byte z;
    clm_byte width;
    clm_byte height;
    clm_byte depth;
} clm_block_3d_u8;
typedef clm_block_3d_u8 clm_block_3d_byte;

typedef struct {
    clm_short x;
    clm_short y;
    clm_short z;
    clm_short width;
    clm_short height;
    clm_short depth;
} clm_block_3d_i16;
typedef clm_block_3d_i16 clm_block_3d_short;

typedef struct {
    clm_ushort x;
    clm_ushort y;
    clm_ushort z;
    clm_ushort width;
    clm_ushort height;
    clm_ushort depth;
} clm_block_3d_u16;
typedef clm_block_3d_u16 clm_block_3d_ushort;

typedef struct {
    clm_int x;
    clm_int y;
    clm_int z;
    clm_int width;
    clm_int height;
    clm_int depth;
} clm_block_3d_i32;
typedef clm_block_3d_i32 clm_block_3d;
typedef clm_block_3d_i32 clm_block_3d_int;

typedef struct {
    clm_uint x;
    clm_uint y;
    clm_uint z;
    clm_uint width;
    clm_uint height;
    clm_uint depth;
} clm_block_3d_u32;
typedef clm_block_3d_u32 clm_block_3d_uint;

typedef struct {
    clm_long x;
    clm_long y;
    clm_long z;
    clm_long width;
    clm_long height;
    clm_long depth;
} clm_block_3d_i64;
typedef clm_block_3d_i64 clm_block_3d_long;

typedef struct {
    clm_ulong x;
    clm_ulong y;
    clm_ulong z;
    clm_ulong width;
    clm_ulong height;
    clm_ulong depth;
} clm_block_3d_u64;
typedef clm_block_3d_u64 clm_block_3d_ulong;

typedef struct {
    clm_float x;
    clm_float y;
    clm_float z;
    clm_float width;
    clm_float height;
    clm_float depth;
} clm_block_3d_f32;
typedef clm_block_3d_f32 clm_block_3d_float;

typedef struct {
    clm_double x;
    clm_double y;
    clm_double z;
    clm_double width;
    clm_double height;
    clm_double depth;
} clm_block_3d_f64;
typedef clm_block_3d_f64 clm_block_3d_double;