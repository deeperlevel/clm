# frozen_string_literal: true

require 'minitest'
require_relative '../clm/base'

class ClmBaseTest < CLMTestCase
  CLM_TEST_TITLE = 'CLM - Base'

  def test_mutable_default_records
    position = CLM::Base::Position.new(x: 10, y: 20)
    position.x = 30

    assert_equal 30, position.x
    assert_equal 20, position.y
    position_3d = CLM::Base::Position3D.new(x: 10, y: 20, z: 30)
    assert_equal [10, 20, 30], position_3d.to_a

    size = CLM::Base::Size.new(width: 100, height: 200)
    size.width = 300
    assert_equal [300, 200], size.to_a
    assert_equal [100, 200, 300], CLM::Base::Size3D.new(width: 100, height: 200, depth: 300).to_a

    assert_equal [10, 20, 100, 200], CLM::Base::Block.new(x: 10, y: 20, width: 100, height: 200).to_a
    assert_equal [10, 20, 30, 100, 200, 300], CLM::Base::Block3D.new(x: 10, y: 20, z: 30, width: 100, height: 200, depth: 300).to_a
  end

  def test_numeric_record_aliases
    families = {
      Position: %i[x y],
      Position3D: %i[x y z],
      Size: %i[width height],
      Size3D: %i[width height depth],
      Block: %i[x y width height],
      Block3D: %i[x y z width height depth]
    }
    suffixes = {
      I8: :SByte,
      U8: :Byte,
      I16: :Short,
      U16: :UShort,
      I32: :Int,
      U32: :UInt,
      I64: :Long,
      U64: :ULong,
      F32: :Float,
      F64: :Double
    }

    families.each do |family, fields|
      suffixes.each do |suffix, alias_name|
        record_type = CLM::Base.const_get("#{family}#{suffix}")
        alias_type = CLM::Base.const_get("#{family}#{alias_name}")

        assert_same record_type, alias_type
        assert_equal fields, record_type.members
      end

      assert_same CLM::Base.const_get("#{family}I32"), CLM::Base.const_get(family)
    end
  end

  def test_unsigned_64_bit_id_range
    assert CLM::Base.valid_id?(0)
    assert CLM::Base.valid_id?(CLM::Base::ID_MAX)
    refute CLM::Base.valid_id?(-1)
    refute CLM::Base.valid_id?(CLM::Base::ID_MAX + 1)
    refute CLM::Base.valid_id?(1.0)
  end
end