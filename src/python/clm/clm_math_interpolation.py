# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by Deeperlevel, developed with community contributions.
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

"""Scalar and Storage interpolation, easing, cubic curves."""
from clm_math import clm_math_lerp,clm_math_inverse_lerp,clm_math_smoothstep
from clm_math_vector import clm_math_vector_lerp

def clm_math_interpolation_lerp(a,b,t):return clm_math_lerp(a,b,t)
def clm_math_interpolation_inverse_lerp(a,b,v):return clm_math_inverse_lerp(a,b,v)
def clm_math_interpolation_smoothstep(a,b,v):return clm_math_smoothstep(a,b,v)
def clm_math_interpolation_ease_in(t):return t*t
def clm_math_interpolation_ease_out(t):return 1-(1-t)*(1-t)
def clm_math_interpolation_ease_in_out(t):return t*t*(3-2*t)
def clm_math_interpolation_cubic(a,b,c,d,t):
    """Catmull-Rom scalar interpolation; t is not clamped."""
    return .5*((2*b)+(-a+c)*t+(2*a-5*b+4*c-d)*t*t+(-a+3*b-3*c+d)*t*t*t)
def clm_math_interpolation_storage_lerp(first,second,t,destination):return clm_math_vector_lerp(first,second,t,destination)
