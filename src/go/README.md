# CLM Go

Go counterparts of the C base, BPA, console, dictionary, ID, string, and storage
headers live in `pkg/clm`. Exported names use PascalCase without the redundant
`clm_` prefix: `clm_string_new` becomes `clm.StringNew`, for example.

## Run

From this directory:

```sh
go run ./cmd
go test ./... github.com/deeperlevel/clm/src/go/tests
```

The command runs all seven suites in the C runner's order, writes an uncolored
`main.log`, prints a combined summary, and exits nonzero on assertions or log
failures. The existing nested `tests` module is resolved locally by `go.mod`.

## Go Conventions

- Buffer APIs accept byte slices instead of C pointers plus separate sizes.
- Output parameters become `(value, status)` return values. Existing status
  numbers and boolean-returning APIs are retained.
- BPA byte-string buffers include a NUL terminator. Higher-level `String*`
  functions take ordinary Go strings, reject embedded NUL and malformed UTF-8,
  and index Unicode code points rather than bytes.
- Fixed-width binary values use byte slices. Use `encoding/binary` for numeric
  transfers; managed numeric metadata uses little-endian bytes.
- Dictionaries use native Go hash maps plus ordered keys and BPA array values.
  The ID registry uses native maps with the C ASCII case-folding policy.
- Scalar blocks reuse zeroed size-class slabs. Arrays and string-array values
  use Go-managed memory rather than C allocations. Borrowed slices alias their
  source; do not retain them across mutation, resize, deletion, or shutdown.
- Global registries and runtime defaults are not safe for concurrent mutation,
  matching the C binding. Advanced array handles are explicit retained borrows.
- Geometry has fixed-width and C-style aliases. `PositionByte` is unsigned,
  correcting the C header's accidental signed alias.

C-only preprocessor implementation switches, private hash-table machinery, and
raw pointer fields have no Go equivalent.