# clm_math_number — Number theory and modular arithmetic

CLM — Cross Language Modules · CC0 1.0 Universal · https://github.com/deeperlevel/clm

## Scope

Number theory and modular arithmetic. Python reference implementation; a language-agnostic functional contract, not a requirement to match the Python implementation strategy.

## Data representation

Ordinary integer functions use scalar integer arguments. Factorization writes results to caller-created LONG_INTEGER Storage, ordered ascending with multiplicity.

**No public math operation receives an array, buffer, raw pointer, dictionary, or mutable vector object.** Structured inputs and outputs are identified by developer-assigned CLM Storage IDs. A function can return a single scalar, a `(status, scalar)` pair for resource-backed queries, or an integer status for mutations. Python scalar tuples (such as extended-GCD coefficients) are returned by value, not passed as buffers.

## Behavioral contract

Modulo is Euclidean, for positive modulus m its result is [0,m-1]. Extended GCD returns nonnegative gcd with Bezout coefficients. Negative modular exponents are rejected. Prime checking is trial division and NOT intended as a large-integer cryptographic primality test. Python arbitrary-precision integer behavior is not portable to C fixed-width types; implementations must detect overflow or implement explicit big integers. No constant-time/security guarantees.

For all Storage operations, the destination must be previously created as `STORAGE_TYPE_DOUBLE` (or `STORAGE_TYPE_LONG_INTEGER` where explicitly stated), writable, and of linear kind. Result length may be resized automatically. Operations read all source values before writing, so source/destination ID aliasing is supported. Errors arising before a write leave the destination unchanged. All Storage resources are owned by the caller; math functions neither create nor destroy them.

## Functions

- `clm_math_number_gcd(a, b)`
- `clm_math_number_lcm(a, b)`
- `clm_math_number_extended_gcd(a, b)`
- `clm_math_number_mod(a, m)`
- `clm_math_number_mod_add(a, b, m)`
- `clm_math_number_mod_sub(a, b, m)`
- `clm_math_number_mod_mul(a, b, m)`
- `clm_math_number_mod_pow(a, e, m)`
- `clm_math_number_mod_inverse(a, m)`
- `clm_math_number_is_coprime(a, b)`
- `clm_math_number_is_even(n)`
- `clm_math_number_is_odd(n)`
- `clm_math_number_is_prime(n)`
- `clm_math_number_next_prime(n)`
- `clm_math_number_prime_factors(n, destination)`

## Error handling

Storage-based mutation functions return `MATH_SUCCESS=0`, `MATH_ERROR_RESOURCE=1`, `MATH_ERROR_DIMENSION=2`, `MATH_ERROR_DOMAIN=3`, `MATH_ERROR_VALUE=4`, `MATH_ERROR_SINGULAR=5`, or `MATH_ERROR_NOT_SUPPORTED=6`. Resource-based scalar queries return `(status, value)` and `None` as the value on failure. Scalar-only functions in the Python reference generally return their result directly and raise native exceptions for invalid values. A cross-language scalar error ABI must be explicitly designed if exception-free C compatibility is required; do not assume exceptions cross a C ABI.

## Numerical portability

DOUBLE is binary64; floating-point results should be compared using documented tolerances, not bitwise identity. Trigonometric/elementary math follows native IEEE-754-capable runtime behavior and may vary in last bits. Dimensions, coordinate conventions, and output ordering MUST remain identical across ports.

## Security

This is mathematical utility code, not an audited security implementation. Do not use general-purpose algorithms here as drop-in constant-time cryptographic primitives.
