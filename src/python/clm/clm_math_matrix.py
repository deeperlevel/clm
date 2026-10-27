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

"""Row-major DOUBLE Storage matrices. Dimensions supplied explicitly."""

from _clm_math_core import *

def _matrix(sid,rows,cols):
    if not isinstance(rows,int) or not isinstance(cols,int) or rows<=0 or cols<=0:
        return MATH_ERROR_DIMENSION,None
    return read(sid,rows*cols)
def clm_math_matrix_identity(destination,size):
    if type(size)!=int or size<=0:return MATH_ERROR_DIMENSION
    return write(destination,[1. if r==c else 0. for r in range(size) for c in range(size)])
def clm_math_matrix_multiply(first,rows_a,cols_a,second,rows_b,cols_b,destination):
    if cols_a!=rows_b:return MATH_ERROR_DIMENSION
    s,a=_matrix(first,rows_a,cols_a)
    if s:return s
    s,b=_matrix(second,rows_b,cols_b)
    if s:return s
    return write(destination,[sum(a[r*cols_a+k]*b[k*cols_b+c] for k in range(cols_a)) for r in range(rows_a) for c in range(cols_b)])
def clm_math_matrix_transpose(source,rows,cols,destination):
    s,a=_matrix(source,rows,cols)
    return s if s else write(destination,[a[r*cols+c] for c in range(cols) for r in range(rows)])
def clm_math_matrix_transform_vector(matrix,rows,cols,vector,destination):
    s,a=_matrix(matrix,rows,cols)
    if s:return s
    s,b=read(vector,cols)
    if s:return s
    return write(destination,[sum(a[r*cols+c]*b[c] for c in range(cols)) for r in range(rows)])
def _eliminate(matrix,n,inverse=False):
    a=[matrix[r*n:(r+1)*n]+([float(r==c) for c in range(n)] if inverse else []) for r in range(n)]
    width=n*2 if inverse else n
    determinant=1.
    for col in range(n):
        pivot=max(range(col,n),key=lambda r:abs(a[r][col]))
        if a[pivot][col]==0:raise ValueError('singular')
        if pivot!=col:
            a[col],a[pivot]=a[pivot],a[col]
            determinant=-determinant
        v=a[col][col]
        determinant*=v
        if inverse:
            a[col]=[x/v for x in a[col]]
            v=1.
        for r in range(col+1 if not inverse else 0,n):
            if r==col:continue
            factor=a[r][col]/v
            for j in range(col,width):a[r][j]-=factor*a[col][j]
    return [value for row in a for value in row[n:]] if inverse else determinant
def clm_math_matrix_determinant(source,size):
    s,a=_matrix(source,size,size)
    if s:return s,None
    try:return MATH_SUCCESS,_eliminate(a,size)
    except ValueError:return MATH_SUCCESS,0.
def clm_math_matrix_inverse(source,size,destination):
    s,a=_matrix(source,size,size)
    if s:return s
    try:return write(destination,_eliminate(a,size,True))
    except ValueError:return MATH_ERROR_SINGULAR

def clm_math_matrix_rotation2d(angle,destination):
    c,s=math.cos(angle),math.sin(angle)
    return write(destination,[c,-s,s,c])
def clm_math_matrix_translation2d(x,y,destination):
    return write(destination,[1.,0.,float(x),0.,1.,float(y),0.,0.,1.])
def clm_math_matrix_scale2d(x,y,destination):
    return write(destination,[float(x),0.,0.,0.,float(y),0.,0.,0.,1.])
