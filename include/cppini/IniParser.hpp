/**
 * @file IniParser.hpp
 * @brief Single-header, C++17 templated INI parser and in-memory store.
 *
 * @author  Anand
 * @copyright 2026 TruChipTech — MIT License
 * @date    2026-06-28
 * @version 1.0.0
 *
 * CPPIniParser is a header-only re-imagining of the embedded C
 * `SimpleIniParser`. It keeps the same feature set — `[sections]`, a global
 * section, `key = value` / `key : value`, `;`/`#` comments (whole-line and
 * inline), single/double quoted values with escapes, empty values, and
 * LF/CRLF/CR line endings — but expresses it with modern C++17:
 *
 * - **Header-only**: just `#include "cppini/IniParser.hpp"`; no `.a`/`.so`.
 * - **Templated typed access**: `doc.get<int>("net", "port", 80)` resolves the
 *   conversion at compile time via `if constexpr`.
 * - **`std::string_view`-based parsing**: the input buffer is scanned without
 *   per-line heap churn, keeping it fast and compact.
 *
 * @par Quick start
 * @code
 *   #include "cppini/IniParser.hpp"
 *
 *   cppini::IniDocument doc;
 *   doc.loadString("[net]\nhost = example.com\nport = 8080\nup = yes\n");
 *
 *   auto host = doc.get<std::string>("net", "host", "localhost");
 *   auto port = doc.get<int>("net", "port", 80);
 *   bool up   = doc.get<bool>("net", "up", false);
 *
 *   doc.set("net", "port", 9090);           // overwrite
 *   doc.set("net", "gateway", "10.0.0.1");  // add
 * @endcode
 */

#ifndef CPPINI_INIPARSER_HPP
#define CPPINI_INIPARSER_HPP

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/** @brief Public namespace for the CPPIniParser library. */
namespace cppini {

/** @brief Library version string (matches the Git tag). */
inline constexpr const char* kVersion = "1.0.0";

/** @brief Result / error codes returned by parsing and mutation calls. */
enum class Status {
    Ok = 0,        /**< Success. */
    SyntaxError,   /**< Malformed line in the input (e.g. unterminated `[`). */
    NotFound,      /**< Requested section/key does not exist. */
};

/**
 * @brief Human-readable description of a ::Status code.
 * @param status Status value to describe.
 * @return A static, never-null C-string.
 */
inline const char* toString(Status status) noexcept {
    switch (status) {
        case Status::Ok:          return "ok";
        case Status::SyntaxError: return "syntax error";
        case Status::NotFound:    return "key not found";
    }
    return "unknown error";
}

/**
 * @brief One stored key/value pair, tagged with its section.
 *
 * The global section (keys appearing before any `[header]`) is represented by
 * an empty @ref section string.
 */
struct Entry {
    std::string section;  /**< Section name; empty for the global section. */
    std::string key;      /**< Key name. */
    std::string value;    /**< Raw value text (quotes/comments already stripped). */
};

/**
 * @brief In-memory INI document: parse text, read typed values, mutate entries.
 *
 * Entries are stored in insertion order, so @ref dump() round-trips a document
 * in a stable, predictable layout. Lookups are **case-sensitive** for both
 * section and key names, matching the original SimpleIniParser semantics.
 */
class IniDocument {
public:
    /** @brief Construct an empty document. */
    IniDocument() = default;

    /* ------------------------------------------------------------------ *
     * Loading
     * ------------------------------------------------------------------ */

    /**
     * @brief Parse INI text, appending its entries to this document.
     *
     * Existing entries are preserved, so the same document may be loaded into
     * repeatedly to merge several sources (later keys overwrite earlier ones).
     * Call @ref clear() first for a clean parse.
     *
     * @param text Read-only INI text. May contain LF, CRLF or lone-CR endings.
     * @return ::Status::Ok, or ::Status::SyntaxError on a malformed line.
     */
    Status loadString(std::string_view text) {
        std::string section;  // current section; "" == global
        std::size_t i = 0;
        const std::size_t n = text.size();

        while (i < n) {
            std::size_t start = i;
            while (i < n && text[i] != '\n' && text[i] != '\r') {
                ++i;
            }
            Status rc = handleLine(text.substr(start, i - start), section);
            if (rc != Status::Ok) {
                return rc;
            }
            // Advance past the line terminator (LF, CR or CRLF).
            if (i < n && text[i] == '\r') {
                ++i;
                if (i < n && text[i] == '\n') {
                    ++i;
                }
            } else if (i < n && text[i] == '\n') {
                ++i;
            }
        }
        return Status::Ok;
    }

    /**
     * @brief Read a file from disk and @ref loadString() its contents.
     * @param path Filesystem path to an INI file.
     * @return ::Status::Ok, ::Status::NotFound if the file cannot be opened,
     *         or ::Status::SyntaxError on a malformed line.
     */
    Status loadFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return Status::NotFound;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        return loadString(ss.str());
    }

    /* ------------------------------------------------------------------ *
     * Queries
     * ------------------------------------------------------------------ */

    /** @brief Number of stored entries. */
    std::size_t size() const noexcept { return entries_.size(); }

    /** @brief True if the document holds no entries. */
    bool empty() const noexcept { return entries_.empty(); }

    /** @brief Remove every entry, leaving an empty document. */
    void clear() noexcept { entries_.clear(); }

    /** @brief Read-only access to the underlying entries (insertion order). */
    const std::vector<Entry>& entries() const noexcept { return entries_; }

    /**
     * @brief Test whether @p section / @p key exists.
     * @param section Section name (`""` for the global section).
     * @param key     Key name.
     */
    bool has(std::string_view section, std::string_view key) const noexcept {
        return find(section, key) != nullptr;
    }

    /**
     * @brief Fetch a value if present, without a fallback.
     * @param section Section name (`""` for the global section).
     * @param key     Key name.
     * @return The raw string value, or `std::nullopt` if absent.
     */
    std::optional<std::string_view> getOptional(std::string_view section,
                                                std::string_view key) const noexcept {
        if (const Entry* e = find(section, key)) {
            return std::string_view{e->value};
        }
        return std::nullopt;
    }

    /**
     * @brief Typed value lookup with a fallback.
     *
     * The return type @p T selects the conversion at compile time via
     * `if constexpr`:
     * - `bool` — accepts `true/false`, `yes/no`, `on/off`, `1/0`
     *   (case-insensitive);
     * - any integral type — decimal or `0x`-prefixed hexadecimal;
     * - any floating-point type — decimal with optional `e`/`E` exponent;
     * - `std::string` / `std::string_view` — the raw value.
     *
     * @tparam T        Desired result type.
     * @param section   Section name (`""` for the global section).
     * @param key       Key name.
     * @param fallback  Value returned when the key is absent or unconvertible.
     * @return The converted value, or @p fallback.
     */
    template <typename T>
    T get(std::string_view section, std::string_view key, T fallback) const {
        const Entry* e = find(section, key);
        if (e == nullptr) {
            return fallback;
        }
        const std::string& v = e->value;

        if constexpr (std::is_same_v<T, bool>) {
            return parseBool(v, fallback);
        } else if constexpr (std::is_same_v<T, std::string> ||
                             std::is_same_v<T, std::string_view>) {
            return T{v};
        } else if constexpr (std::is_integral_v<T>) {
            return static_cast<T>(parseInt(v, static_cast<long long>(fallback)));
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(parseDouble(v, static_cast<double>(fallback)));
        } else {
            static_assert(sizeof(T) == 0,
                          "cppini::IniDocument::get<T>: unsupported type T");
        }
    }

    /** @brief Convenience wrapper for `get<std::string>`. */
    std::string getString(std::string_view section, std::string_view key,
                          std::string_view fallback = {}) const {
        return get<std::string>(section, key, std::string{fallback});
    }

    /** @brief Convenience wrapper for `get<long>`. */
    long getInt(std::string_view section, std::string_view key,
                long fallback = 0) const {
        return get<long>(section, key, fallback);
    }

    /** @brief Convenience wrapper for `get<double>`. */
    double getDouble(std::string_view section, std::string_view key,
                    double fallback = 0.0) const {
        return get<double>(section, key, fallback);
    }

    /** @brief Convenience wrapper for `get<bool>`. */
    bool getBool(std::string_view section, std::string_view key,
                bool fallback = false) const {
        return get<bool>(section, key, fallback);
    }

    /* ------------------------------------------------------------------ *
     * Mutation
     * ------------------------------------------------------------------ */

    /**
     * @brief Set @p key in @p section, overwriting in place or appending.
     * @param section Section name (`""` for the global section).
     * @param key     Key name.
     * @param value   New string value.
     */
    void set(std::string_view section, std::string_view key,
             std::string_view value) {
        if (Entry* e = findMutable(section, key)) {
            e->value.assign(value);
            return;
        }
        entries_.push_back(Entry{std::string{section}, std::string{key},
                                 std::string{value}});
    }

    /**
     * @brief Set @p key to any arithmetic value (formatted as text).
     *
     * `bool` is stored as `"true"`/`"false"`; other arithmetic types use the
     * standard `std::to_string` decimal formatting.
     *
     * @tparam T      Arithmetic type (integral, floating point, or `bool`).
     * @param section Section name (`""` for the global section).
     * @param key     Key name.
     * @param value   Value to store.
     */
    template <typename T,
              typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    void setValue(std::string_view section, std::string_view key, T value) {
        if constexpr (std::is_same_v<T, bool>) {
            set(section, key, value ? "true" : "false");
        } else {
            set(section, key, std::to_string(value));
        }
    }

    /**
     * @brief Remove @p key from @p section.
     * @param section Section name (`""` for the global section).
     * @param key     Key name.
     * @return ::Status::Ok, or ::Status::NotFound if the key is absent.
     */
    Status remove(std::string_view section, std::string_view key) {
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->section == section && it->key == key) {
                entries_.erase(it);
                return Status::Ok;
            }
        }
        return Status::NotFound;
    }

    /* ------------------------------------------------------------------ *
     * Serialization
     * ------------------------------------------------------------------ */

    /**
     * @brief Render the document back to canonical INI text.
     *
     * Global-section keys are emitted first, then each named section under its
     * `[header]`. The output re-parses to an equivalent document.
     * @return INI text terminated by a trailing newline (empty if no entries).
     */
    std::string dump() const {
        std::string out;
        // Global section first.
        emitSection(out, "", /*header=*/false);
        // Named sections in first-seen order.
        std::vector<std::string_view> seen;
        for (const Entry& e : entries_) {
            if (e.section.empty()) {
                continue;
            }
            bool already = false;
            for (std::string_view s : seen) {
                if (s == e.section) { already = true; break; }
            }
            if (!already) {
                seen.push_back(e.section);
                if (!out.empty()) {
                    out.push_back('\n');
                }
                emitSection(out, e.section, /*header=*/true);
            }
        }
        return out;
    }

private:
    std::vector<Entry> entries_;

    /* ---------------- lookup ---------------- */

    const Entry* find(std::string_view section,
                      std::string_view key) const noexcept {
        for (const Entry& e : entries_) {
            if (e.section == section && e.key == key) {
                return &e;
            }
        }
        return nullptr;
    }

    Entry* findMutable(std::string_view section, std::string_view key) noexcept {
        for (Entry& e : entries_) {
            if (e.section == section && e.key == key) {
                return &e;
            }
        }
        return nullptr;
    }

    /* ---------------- character helpers ---------------- */

    static bool isSpace(char c) noexcept {
        return c == ' ' || c == '\t' || c == '\f' || c == '\v';
    }

    static char lower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    }

    /** @brief Trim leading/trailing blanks, returning a view into @p s. */
    static std::string_view trim(std::string_view s) noexcept {
        std::size_t b = 0, e = s.size();
        while (b < e && isSpace(s[b])) ++b;
        while (e > b && isSpace(s[e - 1])) --e;
        return s.substr(b, e - b);
    }

    /* ---------------- parsing ---------------- */

    /** @brief Strip surrounding quotes / inline comments from a raw value. */
    static std::string cleanValue(std::string_view raw) {
        std::string out;
        std::size_t i = 0;
        while (i < raw.size() && isSpace(raw[i])) ++i;

        if (i < raw.size() && (raw[i] == '"' || raw[i] == '\'')) {
            const char quote = raw[i++];
            while (i < raw.size() && raw[i] != quote) {
                if (quote == '"' && raw[i] == '\\' && i + 1 < raw.size()) {
                    const char esc = raw[i + 1];
                    i += 2;
                    switch (esc) {
                        case 'n':  out.push_back('\n'); break;
                        case 'r':  out.push_back('\r'); break;
                        case 't':  out.push_back('\t'); break;
                        case '\\': out.push_back('\\'); break;
                        case '"':  out.push_back('"');  break;
                        case '\'': out.push_back('\''); break;
                        default:   out.push_back(esc);  break;
                    }
                    continue;
                }
                out.push_back(raw[i++]);
            }
            return out;
        }

        // Unquoted: copy until an inline comment (`;`/`#` at a word boundary).
        for (; i < raw.size(); ++i) {
            const char c = raw[i];
            if ((c == ';' || c == '#') &&
                (out.empty() || isSpace(out.back()))) {
                break;
            }
            out.push_back(c);
        }
        while (!out.empty() && isSpace(out.back())) {
            out.pop_back();
        }
        return out;
    }

    /** @brief Process one logical line, updating @p section as needed. */
    Status handleLine(std::string_view rawLine, std::string& section) {
        std::string_view line = trim(rawLine);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            return Status::Ok;
        }

        if (line[0] == '[') {
            std::size_t close = line.find(']');
            if (close == std::string_view::npos) {
                return Status::SyntaxError;
            }
            section.assign(trim(line.substr(1, close - 1)));
            return Status::Ok;
        }

        std::size_t sep = line.find_first_of("=:");
        if (sep == std::string_view::npos) {
            return Status::SyntaxError;
        }
        std::string_view key = trim(line.substr(0, sep));
        if (key.empty()) {
            return Status::SyntaxError;
        }
        set(section, key, cleanValue(line.substr(sep + 1)));
        return Status::Ok;
    }

    /* ---------------- conversion ---------------- */

    static long long parseInt(const std::string& s, long long fallback) {
        std::size_t i = 0;
        const std::size_t n = s.size();
        while (i < n && isSpace(s[i])) ++i;
        bool neg = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            neg = (s[i] == '-');
            ++i;
        }
        long long val = 0;
        bool any = false;
        if (i + 1 < n && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
            i += 2;
            for (; i < n; ++i) {
                int d;
                const char c = s[i];
                if (c >= '0' && c <= '9')      d = c - '0';
                else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
                else break;
                val = val * 16 + d;
                any = true;
            }
        } else {
            for (; i < n && s[i] >= '0' && s[i] <= '9'; ++i) {
                val = val * 10 + (s[i] - '0');
                any = true;
            }
        }
        if (!any) {
            return fallback;
        }
        return neg ? -val : val;
    }

    static double parseDouble(const std::string& s, double fallback) {
        std::size_t i = 0;
        const std::size_t n = s.size();
        while (i < n && isSpace(s[i])) ++i;
        bool neg = false;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            neg = (s[i] == '-');
            ++i;
        }
        double val = 0.0;
        bool any = false;
        for (; i < n && s[i] >= '0' && s[i] <= '9'; ++i) {
            val = val * 10.0 + (s[i] - '0');
            any = true;
        }
        if (i < n && s[i] == '.') {
            ++i;
            double scale = 0.1;
            for (; i < n && s[i] >= '0' && s[i] <= '9'; ++i) {
                val += (s[i] - '0') * scale;
                scale *= 0.1;
                any = true;
            }
        }
        if (!any) {
            return fallback;
        }
        if (i < n && (s[i] == 'e' || s[i] == 'E')) {
            ++i;
            bool eneg = false;
            if (i < n && (s[i] == '+' || s[i] == '-')) {
                eneg = (s[i] == '-');
                ++i;
            }
            int exp = 0;
            for (; i < n && s[i] >= '0' && s[i] <= '9'; ++i) {
                exp = exp * 10 + (s[i] - '0');
            }
            double factor = 1.0;
            for (int k = 0; k < exp; ++k) {
                factor *= 10.0;
            }
            val = eneg ? val / factor : val * factor;
        }
        return neg ? -val : val;
    }

    static bool ieq(std::string_view a, std::string_view b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (lower(a[i]) != lower(b[i])) {
                return false;
            }
        }
        return true;
    }

    static bool parseBool(const std::string& v, bool fallback) noexcept {
        if (ieq(v, "true") || ieq(v, "yes") || ieq(v, "on") || v == "1") {
            return true;
        }
        if (ieq(v, "false") || ieq(v, "no") || ieq(v, "off") || v == "0") {
            return false;
        }
        return fallback;
    }

    /* ---------------- serialization ---------------- */

    void emitSection(std::string& out, std::string_view section,
                     bool header) const {
        if (header) {
            out.push_back('[');
            out.append(section);
            out.append("]\n");
        }
        for (const Entry& e : entries_) {
            if (e.section == section) {
                out.append(e.key);
                out.append(" = ");
                out.append(e.value);
                out.push_back('\n');
            }
        }
    }
};

}  // namespace cppini

#endif  // CPPINI_INIPARSER_HPP
