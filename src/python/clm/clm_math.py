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

"""CLM scalar math: numbers pass by value; portable operations."""
from _clm_math_core import *
CLM_MATH_PI=math.pi
CLM_MATH_TAU=math.tau
CLM_MATH_E=math.e

def clm_math_clamp(value,low,high):
    if low>high:raise ValueError('low must not exceed high')
    return min(max(value,low),high)
def clm_math_lerp(a,b,t):return a+(b-a)*t
def clm_math_inverse_lerp(a,b,value):
    if a==b:raise ValueError('equal endpoints')
    return (value-a)/(b-a)
def clm_math_smoothstep(a,b,t):
    t=clm_math_clamp(clm_math_inverse_lerp(a,b,t),0.,1.)
    return t*t*(3-2*t)
def clm_math_degrees(radians):return math.degrees(radians)
def clm_math_radians(degrees):return math.radians(degrees)
def clm_math_sin(angle):return math.sin(angle)
def clm_math_cos(angle):return math.cos(angle)
def clm_math_tan(angle):return math.tan(angle)
def clm_math_asin(value):return math.asin(value)
def clm_math_acos(value):return math.acos(value)
def clm_math_atan2(y,x):return math.atan2(y,x)
def clm_math_sqrt(value):return math.sqrt(value)
def clm_math_pow(base,exponent):return math.pow(base,exponent)
def clm_math_log(value,base=math.e):return math.log(value,base)
def clm_math_exp(value):return math.exp(value)
def clm_math_floor(value):return math.floor(value)
def clm_math_ceil(value):return math.ceil(value)
def clm_math_abs(value):return abs(value)
def clm_math_sign(value):return (value>0)-(value<0)
def clm_math_wrap(value,low,high):
    if low>=high:raise ValueError('empty interval')
    return low+(value-low)%(high-low)
def clm_math_approximately(a,b,absolute_tolerance=1e-9,relative_tolerance=1e-9):
    return math.isclose(a,b,abs_tol=absolute_tolerance,rel_tol=relative_tolerance)
