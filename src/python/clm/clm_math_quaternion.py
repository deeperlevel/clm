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

"""Quaternions [x,y,z,w] stored in four-element DOUBLE Storage."""

from _clm_math_core import *

def clm_math_quaternion_identity(destination):return write(destination,[0.,0.,0.,1.])
def clm_math_quaternion_set(destination,x,y,z,w):return write(destination,[x,y,z,w])
def clm_math_quaternion_normalize(source,destination):
    def f(q):
        n=math.hypot(*q)
        if n==0:raise ValueError('zero quaternion')
        return [v/n for v in q]
    return unary(source,destination,f,4)
def clm_math_quaternion_conjugate(source,destination):return unary(source,destination,lambda q:[-q[0],-q[1],-q[2],q[3]],4)
def clm_math_quaternion_multiply(first,second,destination):
    # noinspection pep8-naming
    def multiply(a,b):
        x,y,z,w=a;X,Y,Z,W=b
        return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
    return binary(first,second,destination,multiply,4)
def clm_math_quaternion_from_axis_angle(axis,angle,destination):
    s,v=read(axis,3)
    if s:return s
    length=math.hypot(*v)
    if length==0:return MATH_ERROR_DOMAIN
    factor=math.sin(angle/2)/length
    return write(destination,[x*factor for x in v]+[math.cos(angle/2)])
def clm_math_quaternion_rotate_vector(quaternion,vector,destination):
    s,q=read(quaternion,4)
    if s:return s
    s,v=read(vector,3)
    if s:return s
    norm=sum(x*x for x in q)
    if norm==0:return MATH_ERROR_DOMAIN
    x,y,z,w=q
    # Equivalent to q * (v,0) * inverse(q), also works for nonunit q.
    u=[x,y,z]
    dot=sum(a*b for a,b in zip(u,v)); uu=sum(a*a for a in u)
    cross=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
    return write(destination,[((w*w-uu)*v[i]+2*dot*u[i]+2*w*cross[i])/norm for i in range(3)])
def clm_math_quaternion_slerp(first,second,t,destination):
    if not finite(t):return MATH_ERROR_VALUE
    s,a=read(first,4)
    if s:return s
    s,b=read(second,4)
    if s:return s
    na=math.hypot(*a);nb=math.hypot(*b)
    if na==0 or nb==0:return MATH_ERROR_DOMAIN
    a=[v/na for v in a];b=[v/nb for v in b]
    dot=sum(x*y for x,y in zip(a,b))
    if dot<0:dot=-dot;b=[-v for v in b]
    dot=max(-1.,min(1.,dot))
    if dot>.9995:
        q=[x+t*(y-x) for x,y in zip(a,b)]
        norm=math.hypot(*q)
        return write(destination,[x/norm for x in q])
    theta=math.acos(dot);sintheta=math.sin(theta)
    p=math.sin((1-t)*theta)/sintheta;v=math.sin(t*theta)/sintheta
    return write(destination,[p*x+v*y for x,y in zip(a,b)])
