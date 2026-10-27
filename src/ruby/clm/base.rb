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
module Base
  alias :int :ClmId
  Position    = Data.define(:x, :y)
  Position3D  = Data.define(:x, :y, :z)

  Size        = Data.define(:width, :height)
  Size3D      = Data.define(:width, :height, :depth)

  Block       = Data.define(:x, :y, :width, :height)
  Block3D     = Data.define(:x, :y, :z, :width, :height, :depth)
end
