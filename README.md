# CPPIniParser

A **single-header, C++17 INI parser** and in-memory store. Drop one header into
your project, `#include` it, and you get typed configuration access with no
libraries to build, link, or ship — no `.a`, no `.so`.

It is a modern, header-only re-imagining of the embedded C
[`SimpleIniParser`](../SimpleIniParser): the same parsing rules and feature set,
expressed with templates, `if constexpr`, `std::string_view`, and
`std::optional`.

```cpp
#include "cppini/IniParser.hpp"

cppini::IniDocument doc;
doc.loadString("[net]\nhost = example.com\nport = 8080\nup = yes\n");

auto host = doc.get<std::string>("net", "host", "localhost");
auto port = doc.get<int>("net", "port", 80);
bool up   = doc.get<bool>("net", "up", false);

doc.set("net", "port", 9090);          // overwrite
doc.set("net", "gateway", "10.0.0.1"); // add
```

---

## Why header-only?

- **Zero build friction** — copy `include/cppini/IniParser.hpp` and compile.
  No separate library target, no install step required for downstream apps.
- **Inlining across the boundary** — the compiler sees the whole implementation,
  so hot paths (lookup, conversion) inline into your code.
- **Compile-time type dispatch** — `get<T>()` picks the conversion with
  `if constexpr`; there is no runtime type tag and no virtual dispatch.

### Size & performance notes

- Parsing scans the input via `std::string_view` — no per-line heap
  allocations; only the stored values are copied.
- Entries live in a single contiguous `std::vector`, which keeps the footprint
  small and cache-friendly. Lookups are linear, which is the right trade-off for
  the small documents INI files typically hold.
- The default CMake build is `Release` (`-O3`); pair it with `-Os` if you are
  optimizing for code size.

---

## Requirements

- A C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+).
- Only the standard library — `<string>`, `<string_view>`, `<vector>`,
  `<optional>`, `<fstream>`, `<type_traits>`.

---

## Using it in your project

### Copy the header

```cpp
#include "cppini/IniParser.hpp"   // add include/ to your include path
```

### Or via CMake

```cmake
add_subdirectory(CPPIniParser)
target_link_libraries(my_app PRIVATE cppini::cppini)
```

`cppini::cppini` is an `INTERFACE` target — linking it just adds the include
path and requests C++17.

---

## Building the tests & demo

```sh
cmake -S . -B build            # configures a Release build by default
cmake --build build
ctest --test-dir build --output-on-failure   # run unit tests
./build/ini_demo examples/sample.ini         # run the sample app
```

CMake options: `-DCPPINI_BUILD_TESTS=OFF`, `-DCPPINI_BUILD_EXAMPLES=OFF`.

---

## API overview

All symbols live in namespace `cppini`. The core type is `IniDocument`.

### Loading

| Method | Description |
|---|---|
| `Status loadString(std::string_view)` | Parse an in-memory buffer; appends (merges) into the document. |
| `Status loadFile(const std::string&)` | Read a file and parse it. |

### Typed access

| Method | Returns |
|---|---|
| `T get<T>(section, key, fallback)` | Templated typed lookup (string / integral / floating / bool). |
| `getString / getInt / getDouble / getBool` | Convenience wrappers. |
| `std::optional<std::string_view> getOptional(section, key)` | Value if present, else `nullopt`. |
| `bool has(section, key)` | Existence check. |

`bool` accepts `true/false`, `yes/no`, `on/off`, `1/0` (case-insensitive).
Integers accept decimal or `0x` hex. Use `""` as the section for the global
section.

### Mutation & serialization

| Method | Description |
|---|---|
| `void set(section, key, value)` | Add or overwrite a string value. |
| `void setValue<T>(section, key, value)` | Add/overwrite from any arithmetic type. |
| `Status remove(section, key)` | Delete a key (`Status::NotFound` if absent). |
| `void clear()` | Drop all entries. |
| `std::string dump()` | Serialize back to canonical INI text. |
| `size()` / `empty()` / `entries()` | Inspect the store. |

---

## Accepted INI syntax

- `[sections]`, plus a **global section** (`""`) for keys before any header
- `key = value` and `key : value`
- `;` and `#` comment lines, and inline comments after a value
- single (`'...'`) and double (`"..."`) quoted values (preserve spaces /
  comment chars)
- escapes inside double quotes: `\\ \" \n \r \t`
- empty values (`key =`)
- LF, CRLF and lone-CR line endings

Lookups are **case-sensitive** for section and key names.

---

## Project layout

```
include/cppini/IniParser.hpp   the entire library (Doxygen-documented)
examples/ini_demo.cpp          sample application
examples/sample.ini            sample configuration
tests/test_ini.cpp             unit tests (CTest)
CMakeLists.txt                 header-only target + tests/demo + install
Doxyfile                       API documentation config
```

Generate the API docs with `doxygen Doxyfile` (output in `docs/`).

---

## License

MIT — see [LICENSE](LICENSE).
