unit base;

{$mode objfpc}{$H+}
{$PACKRECORDS C}

interface

type
  clm_id = QWord;

  clm_sbyte = ShortInt;
  clm_byte = Byte;
  clm_short = SmallInt;
  clm_ushort = Word;
  clm_int = LongInt;
  clm_uint = LongWord;
  clm_long = Int64;
  clm_ulong = QWord;
  clm_float = Single;
  clm_double = Double;

  clm_position_i8 = record x, y: clm_sbyte; end;
  clm_position_sbyte = clm_position_i8;
  clm_position_u8 = record x, y: clm_byte; end;
  clm_position_byte = clm_position_u8;
  clm_position_i16 = record x, y: clm_short; end;
  clm_position_short = clm_position_i16;
  clm_position_u16 = record x, y: clm_ushort; end;
  clm_position_ushort = clm_position_u16;
  clm_position_i32 = record x, y: clm_int; end;
  clm_position = clm_position_i32;
  clm_position_int = clm_position_i32;
  clm_position_u32 = record x, y: clm_uint; end;
  clm_position_uint = clm_position_u32;
  clm_position_i64 = record x, y: clm_long; end;
  clm_position_long = clm_position_i64;
  clm_position_u64 = record x, y: clm_ulong; end;
  clm_position_ulong = clm_position_u64;
  clm_position_f32 = record x, y: clm_float; end;
  clm_position_float = clm_position_f32;
  clm_position_f64 = record x, y: clm_double; end;
  clm_position_double = clm_position_f64;

  clm_position_3d_i8 = record x, y, z: clm_sbyte; end;
  clm_position_3d_sbyte = clm_position_3d_i8;
  clm_position_3d_u8 = record x, y, z: clm_byte; end;
  clm_position_3d_byte = clm_position_3d_u8;
  clm_position_3d_i16 = record x, y, z: clm_short; end;
  clm_position_3d_short = clm_position_3d_i16;
  clm_position_3d_u16 = record x, y, z: clm_ushort; end;
  clm_position_3d_ushort = clm_position_3d_u16;
  clm_position_3d_i32 = record x, y, z: clm_int; end;
  clm_position_3d = clm_position_3d_i32;
  clm_position_3d_int = clm_position_3d_i32;
  clm_position_3d_u32 = record x, y, z: clm_uint; end;
  clm_position_3d_uint = clm_position_3d_u32;
  clm_position_3d_i64 = record x, y, z: clm_long; end;
  clm_position_3d_long = clm_position_3d_i64;
  clm_position_3d_u64 = record x, y, z: clm_ulong; end;
  clm_position_3d_ulong = clm_position_3d_u64;
  clm_position_3d_f32 = record x, y, z: clm_float; end;
  clm_position_3d_float = clm_position_3d_f32;
  clm_position_3d_f64 = record x, y, z: clm_double; end;
  clm_position_3d_double = clm_position_3d_f64;

  clm_size_i8 = record width, height: clm_sbyte; end;
  clm_size_sbyte = clm_size_i8;
  clm_size_u8 = record width, height: clm_byte; end;
  clm_size_byte = clm_size_u8;
  clm_size_i16 = record width, height: clm_short; end;
  clm_size_short = clm_size_i16;
  clm_size_u16 = record width, height: clm_ushort; end;
  clm_size_ushort = clm_size_u16;
  clm_size_i32 = record width, height: clm_int; end;
  clm_size = clm_size_i32;
  clm_size_int = clm_size_i32;
  clm_size_u32 = record width, height: clm_uint; end;
  clm_size_uint = clm_size_u32;
  clm_size_i64 = record width, height: clm_long; end;
  clm_size_long = clm_size_i64;
  clm_size_u64 = record width, height: clm_ulong; end;
  clm_size_ulong = clm_size_u64;
  clm_size_f32 = record width, height: clm_float; end;
  clm_size_float = clm_size_f32;
  clm_size_f64 = record width, height: clm_double; end;
  clm_size_double = clm_size_f64;

  clm_size_3d_i8 = record width, height, depth: clm_sbyte; end;
  clm_size_3d_sbyte = clm_size_3d_i8;
  clm_size_3d_u8 = record width, height, depth: clm_byte; end;
  clm_size_3d_byte = clm_size_3d_u8;
  clm_size_3d_i16 = record width, height, depth: clm_short; end;
  clm_size_3d_short = clm_size_3d_i16;
  clm_size_3d_u16 = record width, height, depth: clm_ushort; end;
  clm_size_3d_ushort = clm_size_3d_u16;
  clm_size_3d_i32 = record width, height, depth: clm_int; end;
  clm_size_3d = clm_size_3d_i32;
  clm_size_3d_int = clm_size_3d_i32;
  clm_size_3d_u32 = record width, height, depth: clm_uint; end;
  clm_size_3d_uint = clm_size_3d_u32;
  clm_size_3d_i64 = record width, height, depth: clm_long; end;
  clm_size_3d_long = clm_size_3d_i64;
  clm_size_3d_u64 = record width, height, depth: clm_ulong; end;
  clm_size_3d_ulong = clm_size_3d_u64;
  clm_size_3d_f32 = record width, height, depth: clm_float; end;
  clm_size_3d_float = clm_size_3d_f32;
  clm_size_3d_f64 = record width, height, depth: clm_double; end;
  clm_size_3d_double = clm_size_3d_f64;

  clm_block_i8 = record x, y, width, height: clm_sbyte; end;
  clm_block_sbyte = clm_block_i8;
  clm_block_u8 = record x, y, width, height: clm_byte; end;
  clm_block_byte = clm_block_u8;
  clm_block_i16 = record x, y, width, height: clm_short; end;
  clm_block_short = clm_block_i16;
  clm_block_u16 = record x, y, width, height: clm_ushort; end;
  clm_block_ushort = clm_block_u16;
  clm_block_i32 = record x, y, width, height: clm_int; end;
  clm_block = clm_block_i32;
  clm_block_int = clm_block_i32;
  clm_block_u32 = record x, y, width, height: clm_uint; end;
  clm_block_uint = clm_block_u32;
  clm_block_i64 = record x, y, width, height: clm_long; end;
  clm_block_long = clm_block_i64;
  clm_block_u64 = record x, y, width, height: clm_ulong; end;
  clm_block_ulong = clm_block_u64;
  clm_block_f32 = record x, y, width, height: clm_float; end;
  clm_block_float = clm_block_f32;
  clm_block_f64 = record x, y, width, height: clm_double; end;
  clm_block_double = clm_block_f64;

  clm_block_3d_i8 = record x, y, z, width, height, depth: clm_sbyte; end;
  clm_block_3d_sbyte = clm_block_3d_i8;
  clm_block_3d_u8 = record x, y, z, width, height, depth: clm_byte; end;
  clm_block_3d_byte = clm_block_3d_u8;
  clm_block_3d_i16 = record x, y, z, width, height, depth: clm_short; end;
  clm_block_3d_short = clm_block_3d_i16;
  clm_block_3d_u16 = record x, y, z, width, height, depth: clm_ushort; end;
  clm_block_3d_ushort = clm_block_3d_u16;
  clm_block_3d_i32 = record x, y, z, width, height, depth: clm_int; end;
  clm_block_3d = clm_block_3d_i32;
  clm_block_3d_int = clm_block_3d_i32;
  clm_block_3d_u32 = record x, y, z, width, height, depth: clm_uint; end;
  clm_block_3d_uint = clm_block_3d_u32;
  clm_block_3d_i64 = record x, y, z, width, height, depth: clm_long; end;
  clm_block_3d_long = clm_block_3d_i64;
  clm_block_3d_u64 = record x, y, z, width, height, depth: clm_ulong; end;
  clm_block_3d_ulong = clm_block_3d_u64;
  clm_block_3d_f32 = record x, y, z, width, height, depth: clm_float; end;
  clm_block_3d_float = clm_block_3d_f32;
  clm_block_3d_f64 = record x, y, z, width, height, depth: clm_double; end;
  clm_block_3d_double = clm_block_3d_f64;

implementation

end.