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

"""2D geometry: points as two-element DOUBLE Storage IDs."""

from _clm_math_core import *

def clm_math_geometry_distance_point_segment(point,start,end):
    s,p=read(point,2)
    if s:return s,None
    s,a=read(start,2)
    if s:return s,None
    s,b=read(end,2)
    if s:return s,None
    dx=b[0]-a[0];dy=b[1]-a[1];length=dx*dx+dy*dy
    t=max(0.,min(1.,((p[0]-a[0])*dx+(p[1]-a[1])*dy)/length)) if length else 0.
    return MATH_SUCCESS,math.hypot(p[0]-(a[0]+t*dx),p[1]-(a[1]+t*dy))
def clm_math_geometry_orientation(first,second,third):
    s,a=read(first,2)
    if s:return s,None
    s,b=read(second,2)
    if s:return s,None
    s,c=read(third,2)
    if s:return s,None
    area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])
    return MATH_SUCCESS,area
def clm_math_geometry_point_in_polygon(point,polygon):
    """Polygon represented by DOUBLE Storage [x0,y0,...]; boundary counts inside."""
    s,p=read(point,2)
    if s:return s,None
    s,xy=read(polygon)
    if s:return s,None
    if len(xy)<6 or len(xy)%2:return MATH_ERROR_DIMENSION,None
    inside=False;n=len(xy)//2;x,y=p
    for i in range(n):
        j=(i-1)%n
        x1,y1=xy[2*j:2*j+2];x2,y2=xy[2*i:2*i+2]
        cross=(x-x1)*(y2-y1)-(y-y1)*(x2-x1)
        if abs(cross)<=1e-12 and min(x1,x2)<=x<=max(x1,x2) and min(y1,y2)<=y<=max(y1,y2):return MATH_SUCCESS,True
        if (y1>y)!=(y2>y) and x < (x2-x1)*(y-y1)/(y2-y1)+x1:inside=not inside
    return MATH_SUCCESS,inside
def clm_math_geometry_polygon_area(polygon):
    s,xy=read(polygon)
    if s:return s,None
    if len(xy)<6 or len(xy)%2:return MATH_ERROR_DIMENSION,None
    n=len(xy)//2
    return MATH_SUCCESS,abs(sum(xy[2*i]*xy[2*((i+1)%n)+1]-xy[2*((i+1)%n)]*xy[2*i+1] for i in range(n)))/2
def clm_math_geometry_closest_point_segment(point,start,end,destination):
    s,p=read(point,2)
    if s:return s
    s,a=read(start,2)
    if s:return s
    s,b=read(end,2)
    if s:return s
    d=[b[0]-a[0],b[1]-a[1]];dd=sum(v*v for v in d)
    t=max(0.,min(1.,sum((p[i]-a[i])*d[i] for i in range(2))/dd)) if dd else 0
    return write(destination,[a[i]+t*d[i] for i in range(2)])
