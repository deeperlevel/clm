# frozen_string_literal: true

# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm
# Contact: deeparchive@outlook.com
#
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
#
# Free to use, modify, redistribute, and bundle for any purpose.
# Attribution and retention of this notice are appreciated, not required.
#
# Provided AS IS, without warranty or liability.
#
# Don't start from scratch, bro.
# =============================================================================
module CLM
end

module CLM::Base
  ID_MIN = 0
  ID_MAX = (1 << 64) - 1

  RECORD_FAMILIES = {
    'Position' => %i[x y],
    'Position3D' => %i[x y z],
    'Size' => %i[width height],
    'Size3D' => %i[width height depth],
    'Block' => %i[x y width height],
    'Block3D' => %i[x y z width height depth]
  }.freeze

  TYPE_SUFFIXES = {
    'I8' => 'SByte',
    'U8' => 'Byte',
    'I16' => 'Short',
    'U16' => 'UShort',
    'I32' => 'Int',
    'U32' => 'UInt',
    'I64' => 'Long',
    'U64' => 'ULong',
    'F32' => 'Float',
    'F64' => 'Double'
  }.freeze

  def self.valid_id?(value)
    value.is_a?(Integer) && value.between?(ID_MIN, ID_MAX)
  end

  RECORD_FAMILIES.each do |family, fields|
    TYPE_SUFFIXES.each do |suffix, alias_name|
      record = Struct.new(*fields, keyword_init: true)
      const_set("#{family}#{suffix}", record)
      const_set("#{family}#{alias_name}", record)

      if suffix == 'I32'
        const_set(family, record)
      end
    end
  end
end
