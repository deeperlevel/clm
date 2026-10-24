# CLM Position

## Overview

`clm_position` supplies a small, mutable representation of a two-dimensional position. It holds two integer coordinates, `x` and `y`, without defining a coordinate system, origin, units, or geometric operations.

It is intended to be passed between CLM modules and applications—for example, to describe a console cursor position—without requiring an object-oriented framework.

## Data contract

| Representation | Field | Type | Meaning |
| --- | --- | --- | --- |
| `Position` | `x` | Integer | First/horizontal coordinate |
| `Position` | `y` | Integer | Second/vertical coordinate |

Both fields are required on construction and may be changed afterward. Implementations should preserve their integer values without imposing application-specific constraints.

The representation does **not** prescribe whether positive `y` means down or up, whether coordinates are zero-based, or which unit they use. A consuming module defines those conventions. For example, CLM Console may use zero-based terminal-cell positions, but that is a console contract, not a restriction of `Position` itself.

## Language-neutral example

```text
position = Position(x = 12, y = 8)
assert position.x == 12
assert position.y == 8

position.x = 15
assert position.x == 15
```

## Construction and operations

`Position(x, y)` initializes a position. The module defines no standalone `clm_position_*` operations, arithmetic, normalization, bounds checks, or conversion methods.

A language implementation may use a record, struct, data class, or equivalent lightweight value holder. Optional language-native conveniences must not change the meaning of `x` and `y`.

## Equality and mutability

The shared conceptual meaning of a position is its pair of coordinates. Equality-by-value is useful where a language naturally supports it, but equality operators and object identity are **not** part of a mandatory cross-language function API.

The supplied Python implementation is a mutable dataclass: instances compare by field value, and its generated representation includes both coordinates. Those are Python-specific conveniences, not requirements for every language port.

## Constraints and portability

- The shared contract uses integer coordinates. The applicable integer width/range follows the language implementation unless a consuming API imposes stricter limits.
- No automatic bounds checking or coordinate validation is defined.
- The type is not inherently thread-safe when shared and mutated concurrently.
- No allocation, ownership, or inheritance model is mandated.
- Field ordering is `x`, then `y`.

## Python reference behavior and tests

The reference implementation is:

```python
from dataclasses import dataclass

@dataclass
class Position:
    x: int
    y: int
```

Python type annotations do not enforce runtime integer types. There is no default constructor that omits `x` or `y`, and the class has no custom validators. The accompanying `test_clm_position.py` checks construction, field assignment, value equality, and the generated dataclass behavior.

## Compatibility checklist

- [ ] Provides a `Position` representation with `x` and `y` integer fields.
- [ ] Requires both values when constructing a position (or equivalent explicit initialization).
- [ ] Allows fields to be read and updated.
- [ ] Does not impose an origin, unit, or coordinate direction.
- [ ] Does not require reflection, inheritance, or a runtime object system.
