unit test_clm_base;

{$mode objfpc}{$H+}

interface

function clm_base_tests: LongInt;

implementation

uses
  base, test;

procedure CheckSize(var TestResult: clm_test_result; ActualSize, ExpectedSize: SizeUInt; const Description: UTF8String);
begin
  clm_test_assert(TestResult, ActualSize = ExpectedSize, Description);
end;

function clm_base_tests: LongInt;
var
  TestResult: clm_test_result;
  Position: clm_position;
  Position3D: clm_position_3d;
  SizeValue: clm_size;
  Size3D: clm_size_3d;
  BlockValue: clm_block;
  Block3D: clm_block_3d;
  FloatPosition: clm_position_f32;
  FloatPosition3D: clm_position_3d_f32;
  FloatSize: clm_size_f32;
  FloatSize3D: clm_size_3d_f32;
  FloatBlock: clm_block_f32;
  FloatBlock3D: clm_block_3d_f32;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - Base');

  Position.x := 10;
  Position.y := 20;
  clm_test_assert(TestResult, (Position.x = 10) and (Position.y = 20), 'position.x == 10 && position.y == 20');
  Position.x := 30;
  clm_test_assert(TestResult, Position.x = 30, 'position.x == 30');

  Position3D.x := 10;
  Position3D.y := 20;
  Position3D.z := 30;
  clm_test_assert(TestResult, (Position3D.x = 10) and (Position3D.y = 20) and (Position3D.z = 30), 'position_3d.x == 10 && position_3d.y == 20 && position_3d.z == 30');

  SizeValue.width := 100;
  SizeValue.height := 200;
  clm_test_assert(TestResult, (SizeValue.width = 100) and (SizeValue.height = 200), 'size.width == 100 && size.height == 200');
  SizeValue.width := 300;
  clm_test_assert(TestResult, SizeValue.width = 300, 'size.width == 300');

  Size3D.width := 100;
  Size3D.height := 200;
  Size3D.depth := 300;
  clm_test_assert(TestResult, (Size3D.width = 100) and (Size3D.height = 200) and (Size3D.depth = 300), 'size_3d.width == 100 && size_3d.height == 200 && size_3d.depth == 300');

  BlockValue.x := 10;
  BlockValue.y := 20;
  BlockValue.width := 100;
  BlockValue.height := 200;
  clm_test_assert(TestResult, (BlockValue.x = 10) and (BlockValue.y = 20) and (BlockValue.width = 100) and (BlockValue.height = 200), 'block.x == 10 && block.y == 20 && block.width == 100 && block.height == 200');

  Block3D.x := 10;
  Block3D.y := 20;
  Block3D.z := 30;
  Block3D.width := 100;
  Block3D.height := 200;
  Block3D.depth := 300;
  clm_test_assert(TestResult, (Block3D.x = 10) and (Block3D.y = 20) and (Block3D.z = 30), 'block_3d.x == 10 && block_3d.y == 20 && block_3d.z == 30');
  clm_test_assert(TestResult, (Block3D.width = 100) and (Block3D.height = 200) and (Block3D.depth = 300), 'block_3d.width == 100 && block_3d.height == 200 && block_3d.depth == 300');

  FloatPosition.x := 10.5;
  FloatPosition.y := 20.25;
  clm_test_assert(TestResult, (FloatPosition.x = 10.5) and (FloatPosition.y = 20.25), 'f_position.x == 10.5f && f_position.y == 20.25f');

  FloatPosition3D.x := 10.5;
  FloatPosition3D.y := 20.25;
  FloatPosition3D.z := 30.75;
  clm_test_assert(TestResult, (FloatPosition3D.x = 10.5) and (FloatPosition3D.y = 20.25) and (FloatPosition3D.z = 30.75), 'f_position_3d.x == 10.5f && f_position_3d.y == 20.25f && f_position_3d.z == 30.75f');

  FloatSize.width := 100.5;
  FloatSize.height := 200.25;
  clm_test_assert(TestResult, (FloatSize.width = 100.5) and (FloatSize.height = 200.25), 'f_size.width == 100.5f && f_size.height == 200.25f');

  FloatSize3D.width := 100.5;
  FloatSize3D.height := 200.25;
  FloatSize3D.depth := 300.75;
  clm_test_assert(TestResult, (FloatSize3D.width = 100.5) and (FloatSize3D.height = 200.25) and (FloatSize3D.depth = 300.75), 'f_size_3d.width == 100.5f && f_size_3d.height == 200.25f && f_size_3d.depth == 300.75f');

  FloatBlock.x := 10.5;
  FloatBlock.y := 20.25;
  FloatBlock.width := 100.5;
  FloatBlock.height := 200.25;
  clm_test_assert(TestResult, (FloatBlock.x = 10.5) and (FloatBlock.y = 20.25) and (FloatBlock.width = 100.5) and (FloatBlock.height = 200.25), 'f_block.x == 10.5f && f_block.y == 20.25f && f_block.width == 100.5f && f_block.height == 200.25f');

  FloatBlock3D.x := 10.5;
  FloatBlock3D.y := 20.25;
  FloatBlock3D.z := 30.75;
  FloatBlock3D.width := 100.5;
  FloatBlock3D.height := 200.25;
  FloatBlock3D.depth := 300.75;
  clm_test_assert(TestResult, FloatBlock3D.x = 10.5, 'f_block_3d.x == 10.5f');
  clm_test_assert(TestResult, FloatBlock3D.y = 20.25, 'f_block_3d.y == 20.25f');
  clm_test_assert(TestResult, FloatBlock3D.z = 30.75, 'f_block_3d.z == 30.75f');
  clm_test_assert(TestResult, FloatBlock3D.width = 100.5, 'f_block_3d.width == 100.5f');
  clm_test_assert(TestResult, FloatBlock3D.height = 200.25, 'f_block_3d.height == 200.25f');
  clm_test_assert(TestResult, FloatBlock3D.depth = 300.75, 'f_block_3d.depth == 300.75f');

  CheckSize(TestResult, SizeOf(clm_position_i8), 2, 'sizeof(clm_position_i8) == 2');
  CheckSize(TestResult, SizeOf(clm_position_3d_i8), 3, 'sizeof(clm_position_3d_i8) == 3');
  CheckSize(TestResult, SizeOf(clm_size_i8), 2, 'sizeof(clm_size_i8) == 2');
  CheckSize(TestResult, SizeOf(clm_size_3d_i8), 3, 'sizeof(clm_size_3d_i8) == 3');
  CheckSize(TestResult, SizeOf(clm_block_i8), 4, 'sizeof(clm_block_i8) == 4');
  CheckSize(TestResult, SizeOf(clm_block_3d_i8), 6, 'sizeof(clm_block_3d_i8) == 6');
  CheckSize(TestResult, SizeOf(clm_position_u8), 2, 'sizeof(clm_position_u8) == 2');
  CheckSize(TestResult, SizeOf(clm_position_3d_u8), 3, 'sizeof(clm_position_3d_u8) == 3');
  CheckSize(TestResult, SizeOf(clm_size_u8), 2, 'sizeof(clm_size_u8) == 2');
  CheckSize(TestResult, SizeOf(clm_size_3d_u8), 3, 'sizeof(clm_size_3d_u8) == 3');
  CheckSize(TestResult, SizeOf(clm_block_u8), 4, 'sizeof(clm_block_u8) == 4');
  CheckSize(TestResult, SizeOf(clm_block_3d_u8), 6, 'sizeof(clm_block_3d_u8) == 6');
  CheckSize(TestResult, SizeOf(clm_position_i16), 4, 'sizeof(clm_position_i16) == 4');
  CheckSize(TestResult, SizeOf(clm_position_3d_i16), 6, 'sizeof(clm_position_3d_i16) == 6');
  CheckSize(TestResult, SizeOf(clm_size_i16), 4, 'sizeof(clm_size_i16) == 4');
  CheckSize(TestResult, SizeOf(clm_size_3d_i16), 6, 'sizeof(clm_size_3d_i16) == 6');
  CheckSize(TestResult, SizeOf(clm_block_i16), 8, 'sizeof(clm_block_i16) == 8');
  CheckSize(TestResult, SizeOf(clm_block_3d_i16), 12, 'sizeof(clm_block_3d_i16) == 12');
  CheckSize(TestResult, SizeOf(clm_position_u16), 4, 'sizeof(clm_position_u16) == 4');
  CheckSize(TestResult, SizeOf(clm_position_3d_u16), 6, 'sizeof(clm_position_3d_u16) == 6');
  CheckSize(TestResult, SizeOf(clm_size_u16), 4, 'sizeof(clm_size_u16) == 4');
  CheckSize(TestResult, SizeOf(clm_size_3d_u16), 6, 'sizeof(clm_size_3d_u16) == 6');
  CheckSize(TestResult, SizeOf(clm_block_u16), 8, 'sizeof(clm_block_u16) == 8');
  CheckSize(TestResult, SizeOf(clm_block_3d_u16), 12, 'sizeof(clm_block_3d_u16) == 12');
  CheckSize(TestResult, SizeOf(clm_position_i32), 8, 'sizeof(clm_position_i32) == 8');
  CheckSize(TestResult, SizeOf(clm_position_3d_i32), 12, 'sizeof(clm_position_3d_i32) == 12');
  CheckSize(TestResult, SizeOf(clm_size_i32), 8, 'sizeof(clm_size_i32) == 8');
  CheckSize(TestResult, SizeOf(clm_size_3d_i32), 12, 'sizeof(clm_size_3d_i32) == 12');
  CheckSize(TestResult, SizeOf(clm_block_i32), 16, 'sizeof(clm_block_i32) == 16');
  CheckSize(TestResult, SizeOf(clm_block_3d_i32), 24, 'sizeof(clm_block_3d_i32) == 24');
  CheckSize(TestResult, SizeOf(clm_position_u32), 8, 'sizeof(clm_position_u32) == 8');
  CheckSize(TestResult, SizeOf(clm_position_3d_u32), 12, 'sizeof(clm_position_3d_u32) == 12');
  CheckSize(TestResult, SizeOf(clm_size_u32), 8, 'sizeof(clm_size_u32) == 8');
  CheckSize(TestResult, SizeOf(clm_size_3d_u32), 12, 'sizeof(clm_size_3d_u32) == 12');
  CheckSize(TestResult, SizeOf(clm_block_u32), 16, 'sizeof(clm_block_u32) == 16');
  CheckSize(TestResult, SizeOf(clm_block_3d_u32), 24, 'sizeof(clm_block_3d_u32) == 24');
  CheckSize(TestResult, SizeOf(clm_position_i64), 16, 'sizeof(clm_position_i64) == 16');
  CheckSize(TestResult, SizeOf(clm_position_3d_i64), 24, 'sizeof(clm_position_3d_i64) == 24');
  CheckSize(TestResult, SizeOf(clm_size_i64), 16, 'sizeof(clm_size_i64) == 16');
  CheckSize(TestResult, SizeOf(clm_size_3d_i64), 24, 'sizeof(clm_size_3d_i64) == 24');
  CheckSize(TestResult, SizeOf(clm_block_i64), 32, 'sizeof(clm_block_i64) == 32');
  CheckSize(TestResult, SizeOf(clm_block_3d_i64), 48, 'sizeof(clm_block_3d_i64) == 48');
  CheckSize(TestResult, SizeOf(clm_position_u64), 16, 'sizeof(clm_position_u64) == 16');
  CheckSize(TestResult, SizeOf(clm_position_3d_u64), 24, 'sizeof(clm_position_3d_u64) == 24');
  CheckSize(TestResult, SizeOf(clm_size_u64), 16, 'sizeof(clm_size_u64) == 16');
  CheckSize(TestResult, SizeOf(clm_size_3d_u64), 24, 'sizeof(clm_size_3d_u64) == 24');
  CheckSize(TestResult, SizeOf(clm_block_u64), 32, 'sizeof(clm_block_u64) == 32');
  CheckSize(TestResult, SizeOf(clm_block_3d_u64), 48, 'sizeof(clm_block_3d_u64) == 48');
  CheckSize(TestResult, SizeOf(clm_position_f32), 8, 'sizeof(clm_position_f32) == 8');
  CheckSize(TestResult, SizeOf(clm_position_3d_f32), 12, 'sizeof(clm_position_3d_f32) == 12');
  CheckSize(TestResult, SizeOf(clm_size_f32), 8, 'sizeof(clm_size_f32) == 8');
  CheckSize(TestResult, SizeOf(clm_size_3d_f32), 12, 'sizeof(clm_size_3d_f32) == 12');
  CheckSize(TestResult, SizeOf(clm_block_f32), 16, 'sizeof(clm_block_f32) == 16');
  CheckSize(TestResult, SizeOf(clm_block_3d_f32), 24, 'sizeof(clm_block_3d_f32) == 24');
  CheckSize(TestResult, SizeOf(clm_position_f64), 16, 'sizeof(clm_position_f64) == 16');
  CheckSize(TestResult, SizeOf(clm_position_3d_f64), 24, 'sizeof(clm_position_3d_f64) == 24');
  CheckSize(TestResult, SizeOf(clm_size_f64), 16, 'sizeof(clm_size_f64) == 16');
  CheckSize(TestResult, SizeOf(clm_size_3d_f64), 24, 'sizeof(clm_size_3d_f64) == 24');
  CheckSize(TestResult, SizeOf(clm_block_f64), 32, 'sizeof(clm_block_f64) == 32');
  CheckSize(TestResult, SizeOf(clm_block_3d_f64), 48, 'sizeof(clm_block_3d_f64) == 48');

  clm_test_summary(TestResult.passed, TestResult.failed);
  Result := TestResult.failed;
end;

end.