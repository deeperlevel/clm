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

"""2D AABBs [min_x,min_y,max_x,max_y], circles [cx,cy,r]."""

from _clm_math_core import *

def _aabb(sid):
    s,a=read(sid,4)
    if s:return s,None
    if a[0]>a[2] or a[1]>a[3]:return MATH_ERROR_DOMAIN,None
    return MATH_SUCCESS,a
def _circle(sid):
    s,a=read(sid,3)
    if s:return s,None
    if a[2]<0:return MATH_ERROR_DOMAIN,None
    return MATH_SUCCESS,a
def clm_math_collision_aabb_aabb(first,second):
    s,a=_aabb(first)
    if s:return s,None
    s,b=_aabb(second)
    if s:return s,None
    return MATH_SUCCESS,not (a[2]<b[0] or b[2]<a[0] or a[3]<b[1] or b[3]<a[1])
def clm_math_collision_point_aabb(point,box):
    s,p=read(point,2)
    if s:return s,None
    s,b=_aabb(box)
    if s:return s,None
    return MATH_SUCCESS,b[0]<=p[0]<=b[2] and b[1]<=p[1]<=b[3]
def clm_math_collision_circle_circle(first,second):
    s,a=_circle(first)
    if s:return s,None
    s,b=_circle(second)
    if s:return s,None
    return MATH_SUCCESS,math.hypot(a[0]-b[0],a[1]-b[1])<=a[2]+b[2]


# noinspection unresolved-references
def clm_math_collision_circle_aabb(circle,box):
    s,c=_circle(circle)
    if s:return s,None
    s,b=_aabb(box)
    if s:return s,None
    x=max(b[0],min(c[0],b[2]));y=max(b[1],min(c[1],b[3]))
    return MATH_SUCCESS,(x-c[0])**2+(y-c[1])**2<=c[2]**2
def clm_math_collision_point_circle(point,circle):
    s,p=read(point,2)
    if s:return s,None
    s,c=_circle(circle)
    if s:return s,None
    return MATH_SUCCESS,math.hypot(p[0]-c[0],p[1]-c[1])<=c[2]


# noinspection unsupported-operator,unresolved-references
def clm_math_collision_ray_aabb(origin,direction,box):
    """Ray distance >=0 to first AABB contact; None for miss."""
    s,o=read(origin,2)
    if s:return s,None
    s,d=read(direction,2)
    if s:return s,None
    s,b=_aabb(box)
    if s:return s,None
    if not any(d):return MATH_ERROR_DOMAIN,None
    lo=-math.inf;hi=math.inf
    for i in range(2):
        if d[i]==0:
            if not b[i]<=o[i]<=b[i+2]:return MATH_SUCCESS,None
        else:
            a=(b[i]-o[i])/d[i];z=(b[i+2]-o[i])/d[i]
            lo=max(lo,min(a,z));hi=min(hi,max(a,z))
    return MATH_SUCCESS,(max(0,lo) if hi>=max(0,lo) else None)
