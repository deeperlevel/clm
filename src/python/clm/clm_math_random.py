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

"""Explicit ID-based deterministic PRNG (PCG32), NOT cryptographically secure."""
from clm_storage import (clm_storage_get_info,clm_storage_get,
                         clm_storage_replace,STORAGE_TYPE_LONG_INTEGER,STORAGE_SUCCESS)
MASK=(1<<64)-1
MULT=6364136223846793005


# noinspection unresolved-references
def _valid_rng(sid):
    status,info=clm_storage_get_info(sid)
    if status or info.type!=STORAGE_TYPE_LONG_INTEGER or info.kind!='linear' or info.committed!=2 or info.locked:
        raise ValueError('RNG state requires writable LONG_INTEGER Storage with two elements')

def _set(sid,state,inc):
    for i,v in enumerate([state,inc]):
        # 64-bit two's-complement representation in signed Storage
        signed=v if v<1<<63 else v-(1<<64)
        if clm_storage_replace(sid,i,signed)!=STORAGE_SUCCESS:raise ValueError('failed to store RNG state')

def _get(sid):
    _valid_rng(sid)
    return tuple(clm_storage_get(sid,i)[1]&MASK for i in range(2))

def _step(state,inc):
    old=state;state=(old*MULT+inc)&MASK
    x=(((old>>18)^old)>>27)&0xFFFFFFFF;rot=old>>59
    return state,((x>>rot)|(x<<((-rot)&31)))&0xFFFFFFFF


# noinspection unsupported-operator
def clm_math_random_seed(state_storage,seed,sequence=1):
    if type(seed)!=int or type(sequence)!=int:raise TypeError('integer seed/sequence required')
    _valid_rng(state_storage)
    inc=((sequence<<1)|1)&MASK
    state,_=_step(0,inc)
    state=(state+(seed&MASK))&MASK
    state,_=_step(state,inc)
    _set(state_storage,state,inc)

def clm_math_random_uint32(state_storage):
    state,inc=_get(state_storage)
    next_state,value=_step(state,inc)
    _set(state_storage,next_state,inc)
    return value

def clm_math_random_float(state_storage):return clm_math_random_uint32(state_storage)/4294967296.0

def clm_math_random_integer(state_storage,low,high):
    if type(low)!=int or type(high)!=int or low>high or high-low+1>2**32:raise ValueError('invalid inclusive range')
    bound=high-low+1
    threshold=(1<<32)%bound
    while True:
        x=clm_math_random_uint32(state_storage)
        if x>=threshold:return low+x%bound

def clm_math_random_uniform(state_storage,low,high):
    if low>high:raise ValueError('invalid interval')
    return low+(high-low)*clm_math_random_float(state_storage)
