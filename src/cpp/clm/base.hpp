#pragma once

#include <cstdint>
using clm_id = std::uint64_t;

typedef std::int8_t   clm_sbyte;
typedef std::uint8_t  clm_byte;
typedef std::int16_t  clm_short;
typedef std::uint16_t clm_ushort;
typedef std::int32_t  clm_int;
typedef std::uint32_t clm_uint;
typedef std::int64_t  clm_long;
typedef std::uint64_t clm_ulong;
typedef float         clm_float;
typedef double        clm_double;

namespace clm::base {

    template <typename T>
    struct position {
        T x;
        T y;
    };

    template <typename T>
    struct position_3d {
        T x;
        T y;
        T z;
    };

    template <typename T>
    struct size {
        T width;
        T height;
    };

    template <typename T>
    struct size_3d {
        T width;
        T height;
        T depth;
    };

    template <typename T>
    struct block {
        T x;
        T y;
        T width;
        T height;
    };

    template <typename T>
    struct block_3d {
        T x;
        T y;
        T z;
        T width;
        T height;
        T depth;
    };

    namespace i8
    {
        typedef clm::base::position<clm_sbyte> position;
        typedef clm::base::position_3d<clm_sbyte> position_3d;
        typedef clm::base::size<clm_sbyte> size;
        typedef clm::base::size_3d<clm_sbyte> size_3d;
        typedef clm::base::block<clm_sbyte> block;
        typedef clm::base::block_3d<clm_sbyte> block_3d;
    }

    namespace u8
    {
        typedef clm::base::position<clm_byte> position;
        typedef clm::base::position_3d<clm_byte> position_3d;
        typedef clm::base::size<clm_byte> size;
        typedef clm::base::size_3d<clm_byte> size_3d;
        typedef clm::base::block<clm_byte> block;
        typedef clm::base::block_3d<clm_byte> block_3d;
    }

    namespace i16
    {
        typedef clm::base::position<clm_short> position;
        typedef clm::base::position_3d<clm_short> position_3d;
        typedef clm::base::size<clm_short> size;
        typedef clm::base::size_3d<clm_short> size_3d;
        typedef clm::base::block<clm_short> block;
        typedef clm::base::block_3d<clm_short> block_3d;
    }

    namespace u16
    {
        typedef clm::base::position<clm_ushort> position;
        typedef clm::base::position_3d<clm_ushort> position_3d;
        typedef clm::base::size<clm_ushort> size;
        typedef clm::base::size_3d<clm_ushort> size_3d;
        typedef clm::base::block<clm_ushort> block;
        typedef clm::base::block_3d<clm_ushort> block_3d;
    }

    namespace i32
    {
        typedef clm::base::position<clm_int> position;
        typedef clm::base::position_3d<clm_int> position_3d;
        typedef clm::base::size<clm_int> size;
        typedef clm::base::size_3d<clm_int> size_3d;
        typedef clm::base::block<clm_int> block;
        typedef clm::base::block_3d<clm_int> block_3d;
    }

    namespace u32
    {
        typedef clm::base::position<clm_uint> position;
        typedef clm::base::position_3d<clm_uint> position_3d;
        typedef clm::base::size<clm_uint> size;
        typedef clm::base::size_3d<clm_uint> size_3d;
        typedef clm::base::block<clm_uint> block;
        typedef clm::base::block_3d<clm_uint> block_3d;
    }

    namespace i64
    {
        typedef clm::base::position<clm_long> position;
        typedef clm::base::position_3d<clm_long> position_3d;
        typedef clm::base::size<clm_long> size;
        typedef clm::base::size_3d<clm_long> size_3d;
        typedef clm::base::block<clm_long> block;
        typedef clm::base::block_3d<clm_long> block_3d;
    }

    namespace u64
    {
        typedef clm::base::position<clm_ulong> position;
        typedef clm::base::position_3d<clm_ulong> position_3d;
        typedef clm::base::size<clm_ulong> size;
        typedef clm::base::size_3d<clm_ulong> size_3d;
        typedef clm::base::block<clm_ulong> block;
        typedef clm::base::block_3d<clm_ulong> block_3d;
    }

    namespace f32
    {
        typedef clm::base::position<clm_float> position;
        typedef clm::base::position_3d<clm_float> position_3d;
        typedef clm::base::size<clm_float> size;
        typedef clm::base::size_3d<clm_float> size_3d;
        typedef clm::base::block<clm_float> block;
        typedef clm::base::block_3d<clm_float> block_3d;
    }

    namespace f64
    {
        typedef clm::base::position<clm_double> position;
        typedef clm::base::position_3d<clm_double> position_3d;
        typedef clm::base::size<clm_double> size;
        typedef clm::base::size_3d<clm_double> size_3d;
        typedef clm::base::block<clm_double> block;
        typedef clm::base::block_3d<clm_double> block_3d;
    }
}