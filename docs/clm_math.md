## CLM Math: Language-Neutral Contract

This document specifies observable behavior, not syntax or compiler bindings. Keep input order, output values, error cases, and data ownership consistent across CLM language ports. For C signatures, see the headers under `src/c/clm/math*.h`.

### Storage and ownership

Math operations use caller-owned numeric storage through storage IDs; the math modules do not own or manage the underlying storage. Vector and matrix values are double arrays, while integer and random-state operations use signed 64-bit storage. A failed operation reports an error and does not promise a result value unless stated below. Allocated read results belong to the caller.

### Scalars

Clamp, linear interpolation, inverse interpolation, smoothstep, degrees/radians, trigonometric functions, square root/power/exponential/logarithm, floor/ceil/absolute/sign, wrap and approximate equality. Smoothstep clamps the inverse interpolation factor to the closed interval [0,1]. Wrap maps into the half-open interval [low,high). Approximate equality uses `abs(a-b) <= max(abs_tol, rel_tol * max(abs(a),abs(b)))`, with exact-equality shortcut.

### Vectors

Two-, three- and four-element double vectors; addition, subtraction, scaling, dot, magnitude, distance, normalization, three-dimensional cross product, reflection with nonunit normals and element-wise linear interpolation. Zero-length normalization and zero-normal reflection are domain errors. Dimension mismatch is an error.

### Matrices

Row-major double arrays with explicit row/column counts. Identity, multiplication, transpose, matrix×vector, determinant, inverse, two-dimensional rotation (2×2), homogeneous translation (3×3), and homogeneous scaling (3×3). Matrix multiplication uses `A[r,k]*B[k,c]` summed over k. Inverse and determinant use partial-pivot Gaussian elimination. Singular inversion reports an error; singular determinant returns zero. Angles are radians.

### Interpolation

Scalar linear, inverse linear, smoothstep, quadratic ease in/out/in-out, four-point Catmull-Rom interpolation and element-wise vector interpolation. Ease and cubic functions don't clamp their input factor.

### Geometry

Two-dimensional points `[x,y]`; line-segment distance and closest point, signed triangle orientation/cross product, even/odd polygon membership with boundary counted inside, and unsigned shoelace area. Polygons use flattened `[x0,y0,x1,y1,...]` with at least three vertices; a degenerate segment's closest point is its start.

### Collision

AABB `[min_x,min_y,max_x,max_y]`, circle `[center_x,center_y,radius]`, and two-dimensional point/ray queries. Negative radius or reversed box bounds is invalid. Touching edges counts as intersection. A ray with zero direction is invalid. Ray/AABB returns an explicit hit flag plus the nearest nonnegative contact distance; if no intersection occurs, the distance output is unspecified/unchanged.

### 2D dynamics

The physics helpers use +Y as up. Position, velocity, and acceleration are two-element vectors; gravity is supplied as an acceleration vector. A step uses semi-implicit Euler: first update velocity with `(acceleration + gravity) * dt`, then update position with the new velocity times `dt`. The timestep must be finite and nonnegative, and position and velocity must be distinct state values. A jump sets vertical velocity to a nonnegative upward speed; checking whether the character is grounded is the caller's responsibility. Variable-height jumping can scale positive vertical velocity by a factor in [0,1]; descending velocity is unchanged. Collision detection and response remain separate responsibilities.

### Pathfinding

A* searches a caller-provided directed, weighted graph. Nodes can represent tiles, platform regions, or other navigation states; edges can represent walking, drops, ladders, or feasible jump arcs. Edge costs and heuristic estimates must be finite and nonnegative. For an optimal route, the heuristic must be admissible and return zero at the goal. A successful search distinguishes “no route” from an error and returns the route including both start and goal. The graph and any platformer-specific reachability rules remain owned by the caller.

### Quaternions

Four-element `[x,y,z,w]` values. Identity `[0,0,0,1]`, assignment, normalization, conjugation, Hamilton multiplication, axis/angle construction, rotation of three-dimensional vectors, and shortest-arc spherical interpolation (`slerp`). Axis is normalized on construction; vector rotation also supports nonunit quaternions. `slerp` uses normalized endpoints, flips the second quaternion for negative dot product, and falls back to normalized linear interpolation for near-identical orientations. Interpolation parameter is **not clamped**.

### Integer number theory

Signed-integer GCD/LCM, Bézout coefficients, normalized nonnegative remainder for positive modulus, modular addition/subtraction/multiplication/power/inverse, coprimality, parity, primality, next prime strictly greater than the input, and repeated prime factors in ascending order. A modular inverse exists only when GCD equals one. The C implementation targets 64-bit signed values and reports overflow instead of silently wrapping; it is **not a cryptographic primitive**. The prime test is trial division, not a high-performance primality algorithm.

### Random numbers

Explicit user-owned state containing two 64-bit words (`state`, odd increment), PCG32 output transformation and seed sequence. The same seed/stream pair produces the same 32-bit sequence across ports. Floating random numbers are `uint32 / 2^32`, in [0,1). Inclusive integer range selection uses rejection sampling to avoid modulo bias; range width is at most 2^32. **Not cryptographically secure.** Sharing one PRNG state between threads requires application synchronization.
