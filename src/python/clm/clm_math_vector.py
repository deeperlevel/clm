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

"""Vector operations on DOUBLE CLM Storage IDs (2, 3 or 4 components)."""

from _clm_math_core import *

def clm_math_vector_set(destination,*components):
    if len(components) not in (2,3,4):return MATH_ERROR_DIMENSION
    return write(destination,components)
def clm_math_vector_add(first,second,destination):return binary(first,second,destination,lambda a,b:[x+y for x,y in zip(a,b)])
def clm_math_vector_subtract(first,second,destination):return binary(first,second,destination,lambda a,b:[x-y for x,y in zip(a,b)])
def clm_math_vector_scale(source,factor,destination):
    if not finite(factor):return MATH_ERROR_VALUE
    return unary(source,destination,lambda a:[x*factor for x in a])
def clm_math_vector_dot(first,second):
    s,a=read(first)
    if s:return s,None
    s,b=read(second)
    if s:return s,None
    if len(a)!=len(b):return MATH_ERROR_DIMENSION,None
    return MATH_SUCCESS,sum(x*y for x,y in zip(a,b))
def clm_math_vector_length(source):
    s,a=read(source)
    return (s,None) if s else (MATH_SUCCESS,math.hypot(*a))
def clm_math_vector_distance(first,second):
    s,a=read(first)
    if s:return s,None
    s,b=read(second)
    if s:return s,None
    if len(a)!=len(b):return MATH_ERROR_DIMENSION,None
    return MATH_SUCCESS,math.dist(a,b)
def clm_math_vector_normalize(source,destination):
    def norm(a):
        length=math.hypot(*a)
        if length==0:raise ValueError('zero vector')
        return [v/length for v in a]
    return unary(source,destination,norm)
def clm_math_vector_cross(first,second,destination):
    s,a=read(first,3)
    if s:return s
    s,b=read(second,3)
    if s:return s
    return write(destination,[a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]])
def clm_math_vector_reflect(direction,normal,destination):
    s,d=read(direction)
    if s:return s
    s,n=read(normal,len(d))
    if s:return s
    nn=sum(x*x for x in n)
    if nn==0:return MATH_ERROR_DOMAIN
    factor=2*sum(x*y for x,y in zip(d,n))/nn
    return write(destination,[x-factor*y for x,y in zip(d,n)])
def clm_math_vector_lerp(first,second,t,destination):
    if not finite(t):return MATH_ERROR_VALUE
    return binary(first,second,destination,lambda a,b:[x+(y-x)*t for x,y in zip(a,b)])
