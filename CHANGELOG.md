# Changelog

## V1.0.0 — 2026-06-28

- Initial release.
- Single-header, header-only C++17 INI parser (`include/cppini/IniParser.hpp`) —
  no `.a`/`.so` to build or link.
- Templated typed access: `get<T>()` resolves string/integer/floating/bool
  conversions at compile time via `if constexpr`; `setValue<T>()` for arithmetic
  writes.
- Full SimpleIniParser feature parity: `[sections]` + global section,
  `key = value` / `key : value`, `;`/`#` whole-line and inline comments,
  single/double quoted values with escapes, empty values, and LF/CRLF/CR line
  endings.
- `loadString` / `loadFile`, `has`, `getOptional`, `remove`, and `dump`
  round-trip serialization.
- Unit tests (CTest) and a sample application; Doxygen documentation throughout.
