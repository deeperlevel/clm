"""Native Python implementation of CLM generation (cryptographic secure RNG)."""

import math, secrets, struct

GENERATION_SUCCESS=0
GENERATION_FAILURE=1
GENERATION_CASING_MIXED=0
GENERATION_CASING_UPPER=1
GENERATION_CASING_LOWER=2
_SYMBOLS='!@#$%^&*()-_=+[]{};:,.?/|~'

def _range(lo,hi):
    if lo>hi: lo,hi=hi,lo
    return lo,hi
def _integer(lo,hi,bits,signed):
    low=-(1<<(bits-1)) if signed else 0
    high=(1<<(bits-1))-1 if signed else (1<<bits)-1
    if not low<=lo<=high or not low<=hi<=high: raise ValueError('bound out of range')
    lo,hi=_range(lo,hi)
    return lo+secrets.randbelow(hi-lo+1)

def clm_gen_random_i8(min,max): return _integer(min,max,8,True)

def clm_gen_random_u8(min,max): return _integer(min,max,8,False)

def clm_gen_random_i16(min,max): return _integer(min,max,16,True)

def clm_gen_random_u16(min,max): return _integer(min,max,16,False)

def clm_gen_random_i32(min,max): return _integer(min,max,32,True)

def clm_gen_random_u32(min,max): return _integer(min,max,32,False)

def clm_gen_random_i64(min,max): return _integer(min,max,64,True)

def clm_gen_random_u64(min,max): return _integer(min,max,64,False)

def clm_gen_random_f64(min,max):
    if not math.isfinite(min) or not math.isfinite(max): return math.nan
    min,max=_range(min,max)
    if min==max:return min
    unit=secrets.randbits(53)/((1<<53)-1)
    value=(min*(1-unit)+max*unit) if min<0<max else min+(max-min)*unit
    return max if value>max else min if value<min else value

def clm_gen_random_f32(min,max):
    v=clm_gen_random_f64(min,max)
    if math.isnan(v):return v
    try:return struct.unpack('f',struct.pack('f',v))[0]
    except OverflowError:return math.copysign(math.inf,v)

def _case(casing):
    if casing not in (0,1,2):raise ValueError('invalid casing')

def _pick(a,n):
    if n<0:raise ValueError('negative length')
    return ''.join(secrets.choice(a) for _ in range(n))

def clm_gen_hex(length,casing):
    _case(casing)
    s=_pick('0123456789abcdef' if casing!=1 else '0123456789ABCDEF',length)
    return ''.join(c.upper() if secrets.randbelow(2) else c for c in s) if casing==0 else s

def clm_gen_uuid(casing):
    _case(casing)
    b=bytearray(secrets.token_bytes(16));b[6]=(b[6]&15)|64;b[8]=(b[8]&63)|128
    h=b.hex();s=f'{h[:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:]}'
    if casing==1:return s.upper()
    if casing==0:return ''.join(c.upper() if c in 'abcdef' and secrets.randbelow(2) else c for c in s)
    return s

def clm_gen_alphanum(length,casing):
    _case(casing)
    return _pick('0123456789'+('ABCDEFGHIJKLMNOPQRSTUVWXYZ' if casing==1 else 'abcdefghijklmnopqrstuvwxyz' if casing==2 else 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'),length)

def clm_gen_strong_password(length):
    if length<4:raise ValueError('password length must be >= 4')
    groups=('ABCDEFGHIJKLMNOPQRSTUVWXYZ','abcdefghijklmnopqrstuvwxyz','0123456789',_SYMBOLS)
    letters=[secrets.choice(group) for group in groups]
    letters+=list(_pick(''.join(groups),length-4))
    for i in range(len(letters)-1,0,-1):
        j=secrets.randbelow(i+1);letters[i],letters[j]=letters[j],letters[i]
    return ''.join(letters)

def _to(s, buffer):
    b=s.encode('ascii')+b'\0'
    try:
        if len(buffer)<len(b):return GENERATION_FAILURE
        buffer[:len(b)]=b
        return GENERATION_SUCCESS
    except (TypeError,ValueError):return GENERATION_FAILURE

def clm_gen_hex_to(length,casing,buffer):
    try:
        if length<0 or len(buffer)<length+1:return GENERATION_FAILURE
        return _to(clm_gen_hex(length,casing),buffer)
    except ValueError:return GENERATION_FAILURE

def clm_gen_uuid_to(casing,buffer):
    try:
        if len(buffer)<37:
            return GENERATION_FAILURE
        return _to(clm_gen_uuid(casing),buffer)
    except ValueError:
        return GENERATION_FAILURE

def clm_gen_alphanum_to(length,casing,buffer):
    try:
        if length < 0 or len(buffer) < length + 1:
            return GENERATION_FAILURE
        return _to(clm_gen_alphanum(length,casing),buffer)
    except ValueError:
        return GENERATION_FAILURE

def clm_gen_strong_password_to(length,buffer):
    try:
        if length<4 or len(buffer)<length+1:return GENERATION_FAILURE
        return _to(clm_gen_strong_password(length),buffer)
    except ValueError:
        return GENERATION_FAILURE
