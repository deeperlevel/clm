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

from dataclasses import dataclass

id_key = int

@dataclass
class Position:
    x: int
    y: int

@dataclass
class FPosition:
    x: float
    y: float

@dataclass
class Position3D:
    x: int
    y: int
    z: int

@dataclass
class FPosition3D:
    x: float
    y: float
    z: float

@dataclass
class Size:
    width: int
    height: int

@dataclass
class FSize:
    width: float
    height: float

@dataclass
class Size3D:
    width: int
    height: int
    depth: int

@dataclass
class FSize3D:
    width: float
    height: float
    depth: float

@dataclass
class Block:
    x: int
    y: int
    width: int
    height: int

@dataclass
class FBlock:
    x: float
    y: float
    width: float
    height: float

@dataclass
class Block3D:
    x: int
    y: int
    z: int
    width: int
    height: int
    depth: int

@dataclass
class FBlock3D:
    x: float
    y: float
    z: float
    width: float
    height: float
    depth: float

# @dataclass
# class Circle:
#     x: int
#     y: int
#     radius: int
#
# @dataclass
# class CircleF:
#     x: float
#     y: float
#     radius: float
#
# @dataclass
# class Sphere:
#     x: int
#     y: int
#     z: int
#     radius: int
#
# @dataclass
# class SphereF:
#     x: float
#     y: float
#     z: float
#     radius: float
