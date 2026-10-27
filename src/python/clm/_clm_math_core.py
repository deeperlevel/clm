"""Internal CLM math Storage adapter. No list/array public parameters."""
import math
from clm_storage import (STORAGE_SUCCESS, STORAGE_TYPE_DOUBLE, clm_storage_get_info,
                         clm_storage_get, clm_storage_resize, clm_storage_replace)
MATH_SUCCESS=0
MATH_ERROR_RESOURCE=1
MATH_ERROR_DIMENSION=2
MATH_ERROR_DOMAIN=3
MATH_ERROR_VALUE=4
MATH_ERROR_SINGULAR=5
MATH_ERROR_NOT_SUPPORTED=6

def finite(*values):
    return all(type(x) in (int,float) and math.isfinite(x) for x in values)

def read(sid, n=None):
    status, info = clm_storage_get_info(sid)
    if status or info is None or info.type != STORAGE_TYPE_DOUBLE or info.kind != 'linear':
        return MATH_ERROR_RESOURCE, None
    if n is not None and info.committed != n:
        return MATH_ERROR_DIMENSION, None
    values=[]
    for i in range(info.committed):
        status,v=clm_storage_get(sid,i)
        if status: return MATH_ERROR_RESOURCE,None
        values.append(v)
    return MATH_SUCCESS,values

def write(sid, values):
    """Output must be precreated DOUBLE Storage. Validate before mutating."""
    if not all(finite(v) for v in values): return MATH_ERROR_VALUE
    status,info=clm_storage_get_info(sid)
    if status or info is None or info.type != STORAGE_TYPE_DOUBLE or info.kind != 'linear' or info.locked:
        return MATH_ERROR_RESOURCE
    if clm_storage_resize(sid,len(values)) != STORAGE_SUCCESS:return MATH_ERROR_RESOURCE
    for i,v in enumerate(values):
        if clm_storage_replace(sid,i,float(v)) != STORAGE_SUCCESS:return MATH_ERROR_RESOURCE
    return MATH_SUCCESS

def unary(source,dest,fn,n=None):
    s,a=read(source,n)
    if s:return s
    try: return write(dest,fn(a))
    except (ValueError,ZeroDivisionError,OverflowError):return MATH_ERROR_DOMAIN

def binary(aid,bid,dest,fn,n=None):
    s,a=read(aid,n)
    if s:return s
    s,b=read(bid,n)
    if s:return s
    if len(a)!=len(b):return MATH_ERROR_DIMENSION
    try:return write(dest,fn(a,b))
    except (ValueError,ZeroDivisionError,OverflowError):return MATH_ERROR_DOMAIN
