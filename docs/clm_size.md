# CLM Size

## Overview

`clm_size` supplies a small, mutable representation of a two-dimensional size. It stores integer `width` and `height` without prescribing units, minimum values, or layout behavior.

It is intended for use by CLM modules and applications—for example, to return the dimensions of a terminal window—without requiring a particular programming paradigm.

## Data contract

| Representation | Field | Type | Meaning |
| --- | --- | --- | --- |
| `Size` | `width` | Integer | Extent along the first/horizontal dimension |
| `Size` | `height` | Integer | Extent along the second/vertical dimension |

Both fields are required on construction and may be changed afterward.

## Language-neutral example

```text
size = Size(width = 80, height = 24)
assert size.width == 80
assert size.height == 24

size.height = 30
assert size.height == 30
```

## Construction and operations

`Size(width, height)` initializes a size. The module defines no standalone `clm_size_*` functions, arithmetic, scaling, area calculation, clamping, or validation.

Importantly, the representation itself does **not** reject zero or negative dimensions. A consuming API, such as a console window-resize operation, may define positive-size requirements separately.

Implementations may use a record, struct, data class, or equivalent lightweight holder. No special runtime object model or memory allocator is required.

## Equality and mutability

A size is conceptually described by its `width` and `height` values. Languages may offer value comparison as a convenience, but no cross-language equality function is specified.

The supplied Python implementation is a mutable dataclass. Python automatically generates equality-by-field-value and a readable representation. These are reference-language conveniences, not requirements of the language-agnostic contract.

## Constraints and portability

- Dimensions are represented as integers; their range depends on the language implementation unless a consumer defines its own limits.
- Units are consumer-defined (terminal cells, pixels, logical coordinates, etc.).
- Zero and negative values are representable; validity is determined by the consuming operation.
- Concurrent mutation requires coordination by the caller.
- Field ordering is `width`, then `height`.
- No reflection, inheritance, or mandatory OOP is required.

## Python reference behavior and tests

The reference implementation is:

```python
from dataclasses import dataclass

@dataclass
class Size:
    width: int
    height: int
```

Python annotations do not enforce integer types at runtime, and both constructor parameters are required. The accompanying `test_clm_size.py` checks construction, assignment, equality, and generated dataclass behavior.

## Compatibility checklist

- [ ] Provides a `Size` representation with integer `width` and `height` fields.
- [ ] Requires both values on construction (or explicit equivalent initialization).
- [ ] Allows fields to be read and updated.
- [ ] Does not impose global units, positive-only values, or size limits.
- [ ] Can be consumed without a framework-specific object model.
