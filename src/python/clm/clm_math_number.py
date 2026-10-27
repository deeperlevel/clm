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

"""Integer number theory; not a cryptographic security primitive."""
import math

def _integer(*args):
    if not all(type(v) is int for v in args):raise TypeError('integer arguments required')
def clm_math_number_gcd(a,b):
    _integer(a,b);return math.gcd(a,b)
def clm_math_number_lcm(a,b):
    _integer(a,b);return math.lcm(a,b)
def clm_math_number_extended_gcd(a,b):
    _integer(a,b)
    old_r,r=a,b;old_s,s=1,0;old_t,t=0,1
    while r:
        q=old_r//r;old_r,r=r,old_r-q*r;old_s,s=s,old_s-q*s;old_t,t=t,old_t-q*t
    sign=-1 if old_r<0 else 1
    return sign*old_r,sign*old_s,sign*old_t
def clm_math_number_mod(a,m):
    _integer(a,m)
    if m<=0:raise ValueError('modulus must be positive')
    return a%m
def clm_math_number_mod_add(a,b,m):return (clm_math_number_mod(a,m)+clm_math_number_mod(b,m))%m
def clm_math_number_mod_sub(a,b,m):return (clm_math_number_mod(a,m)-clm_math_number_mod(b,m))%m
def clm_math_number_mod_mul(a,b,m):return (clm_math_number_mod(a,m)*clm_math_number_mod(b,m))%m
def clm_math_number_mod_pow(a,e,m):
    _integer(a,e,m)
    if m<=0 or e<0:raise ValueError('positive modulus and nonnegative exponent required')
    return pow(a,e,m)
def clm_math_number_mod_inverse(a,m):
    _integer(a,m)
    if m<=1:raise ValueError('modulus must exceed 1')
    g,x,_=clm_math_number_extended_gcd(a,m)
    if g!=1:raise ValueError('inverse does not exist')
    return x%m
def clm_math_number_is_coprime(a,b):return clm_math_number_gcd(a,b)==1
def clm_math_number_is_even(n):_integer(n);return n%2==0
def clm_math_number_is_odd(n):_integer(n);return n%2!=0
def clm_math_number_is_prime(n):
    _integer(n)
    if n<2:return False
    if n%2==0:return n==2
    d=3
    while d*d<=n:
        if n%d==0:return False
        d+=2
    return True
def clm_math_number_next_prime(n):
    _integer(n);x=n+1
    while not clm_math_number_is_prime(x):x+=1
    return x


# noinspection unresolved-references
def clm_math_number_prime_factors(n,destination):
    """Write prime factors to caller-created LONG_INTEGER Storage ID."""
    from clm_storage import (clm_storage_get_info,clm_storage_resize,clm_storage_replace,
                             STORAGE_TYPE_LONG_INTEGER, STORAGE_SUCCESS)
    _integer(n)
    if n<2:raise ValueError('integer must be >=2')
    factors=[];x=n;d=2
    while d*d<=x:
        while x%d==0:factors.append(d);x//=d
        d=3 if d==2 else d+2
    if x>1:factors.append(x)
    status,info=clm_storage_get_info(destination)
    if status or info.type != STORAGE_TYPE_LONG_INTEGER or info.locked:raise ValueError('destination must be writable LONG_INTEGER Storage')
    if any(v>=2**63 for v in factors):raise OverflowError('factor exceeds 64-bit Storage')
    if clm_storage_resize(destination,len(factors))!=STORAGE_SUCCESS:raise ValueError('resize failed')
    for i,v in enumerate(factors):
        if clm_storage_replace(destination,i,v)!=STORAGE_SUCCESS:raise ValueError('write failed')
    return len(factors)
