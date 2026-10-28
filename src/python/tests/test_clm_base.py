"""Generic geometry record checks using the CLM test reporting style."""

from clm.base import (
    Block,
    Block3D,
    Block3Di32,
    Block3Df64,
    Position,
    Position3D,
    Position3Df32,
    Positionf32,
    Positioni8,
    Positioni32,
    Size,
    Size3D,
)
from tests.test import TestResult, clm_test_assert, clm_test_begin, clm_test_summary


def run_tests() -> int:
    """Run base record checks and return the number of failed assertions."""
    result = TestResult()
    clm_test_begin("CLM - Base")

    position = Positioni32(10, 20)
    position.x = 30
    clm_test_assert(
        result,
        Positioni8.__origin__ is Position
        and Positioni8.__args__ == (int,)
        and position.x == 30
        and position.y == 20,
        "integer generic aliases construct and mutate positions",
    )

    float_position = Positionf32(10.5, 20.25)
    position_3d = Position3D[int](10, 20, 30)
    float_position_3d = Position3Df32(10.5, 20.25, 30.75)
    clm_test_assert(
        result,
        float_position.x == 10.5
        and float_position.y == 20.25
        and position_3d.z == 30
        and float_position_3d.z == 30.75,
        "2D and 3D positions preserve integer and float values",
    )

    size = Size[int](100, 200)
    size_3d = Size3D[int](100, 200, 300)
    clm_test_assert(
        result,
        size.width == 100
        and size.height == 200
        and size_3d.width == 100
        and size_3d.height == 200
        and size_3d.depth == 300,
        "2D and 3D sizes preserve their dimensions",
    )

    block = Block[int](10, 20, 100, 200)
    block_3d = Block3Df64(10.5, 20.25, 30.75, 100.5, 200.25, 300.75)
    clm_test_assert(
        result,
        block.x == 10
        and block.y == 20
        and block.width == 100
        and block.height == 200
        and Block3Di32 == Block3D[int]
        and block_3d.depth == 300.75,
        "2D and 3D blocks preserve integer and float fields",
    )

    return clm_test_summary(result)