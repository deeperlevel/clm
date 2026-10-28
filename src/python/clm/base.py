"""Shared scalar aliases and generic geometry records corresponding to ``base.h``."""

from dataclasses import dataclass
from typing import Generic, TypeAlias, TypeVar

ScalarT = TypeVar("ScalarT", int, float)

clm_id = int
clm_sbyte = int
clm_byte = int
clm_short = int
clm_ushort = int
clm_int = int
clm_uint = int
clm_long = int
clm_ulong = int
clm_float = float
clm_double = float
id_key = int


@dataclass(slots=True)
class Position(Generic[ScalarT]):
    x: ScalarT
    y: ScalarT


@dataclass(slots=True)
class Position3D(Generic[ScalarT]):
    x: ScalarT
    y: ScalarT
    z: ScalarT


@dataclass(slots=True)
class Size(Generic[ScalarT]):
    width: ScalarT
    height: ScalarT


@dataclass(slots=True)
class Size3D(Generic[ScalarT]):
    width: ScalarT
    height: ScalarT
    depth: ScalarT


@dataclass(slots=True)
class Block(Generic[ScalarT]):
    x: ScalarT
    y: ScalarT
    width: ScalarT
    height: ScalarT


@dataclass(slots=True)
class Block3D(Generic[ScalarT]):
    x: ScalarT
    y: ScalarT
    z: ScalarT
    width: ScalarT
    height: ScalarT
    depth: ScalarT


Positioni8: TypeAlias = Position[int]
Positionu8: TypeAlias = Position[int]
Positioni16: TypeAlias = Position[int]
Positionu16: TypeAlias = Position[int]
Positioni32: TypeAlias = Position[int]
Positionu32: TypeAlias = Position[int]
Positioni64: TypeAlias = Position[int]
Positionu64: TypeAlias = Position[int]
Positionf32: TypeAlias = Position[float]
Positionf64: TypeAlias = Position[float]

Position3Di8: TypeAlias = Position3D[int]
Position3Du8: TypeAlias = Position3D[int]
Position3Di16: TypeAlias = Position3D[int]
Position3Du16: TypeAlias = Position3D[int]
Position3Di32: TypeAlias = Position3D[int]
Position3Du32: TypeAlias = Position3D[int]
Position3Di64: TypeAlias = Position3D[int]
Position3Du64: TypeAlias = Position3D[int]
Position3Df32: TypeAlias = Position3D[float]
Position3Df64: TypeAlias = Position3D[float]

Sizei8: TypeAlias = Size[int]
Sizeu8: TypeAlias = Size[int]
Sizei16: TypeAlias = Size[int]
Sizeu16: TypeAlias = Size[int]
Sizei32: TypeAlias = Size[int]
Sizeu32: TypeAlias = Size[int]
Sizei64: TypeAlias = Size[int]
Sizeu64: TypeAlias = Size[int]
Sizef32: TypeAlias = Size[float]
Sizef64: TypeAlias = Size[float]

Size3Di8: TypeAlias = Size3D[int]
Size3Du8: TypeAlias = Size3D[int]
Size3Di16: TypeAlias = Size3D[int]
Size3Du16: TypeAlias = Size3D[int]
Size3Di32: TypeAlias = Size3D[int]
Size3Du32: TypeAlias = Size3D[int]
Size3Di64: TypeAlias = Size3D[int]
Size3Du64: TypeAlias = Size3D[int]
Size3Df32: TypeAlias = Size3D[float]
Size3Df64: TypeAlias = Size3D[float]

Blocki8: TypeAlias = Block[int]
Blocku8: TypeAlias = Block[int]
Blocki16: TypeAlias = Block[int]
Blocku16: TypeAlias = Block[int]
Blocki32: TypeAlias = Block[int]
Blocku32: TypeAlias = Block[int]
Blocki64: TypeAlias = Block[int]
Blocku64: TypeAlias = Block[int]
Blockf32: TypeAlias = Block[float]
Blockf64: TypeAlias = Block[float]

Block3Di8: TypeAlias = Block3D[int]
Block3Du8: TypeAlias = Block3D[int]
Block3Di16: TypeAlias = Block3D[int]
Block3Du16: TypeAlias = Block3D[int]
Block3Di32: TypeAlias = Block3D[int]
Block3Du32: TypeAlias = Block3D[int]
Block3Di64: TypeAlias = Block3D[int]
Block3Du64: TypeAlias = Block3D[int]
Block3Df32: TypeAlias = Block3D[float]
Block3Df64: TypeAlias = Block3D[float]
