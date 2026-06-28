/**
 * @file test_ini.cpp
 * @brief Unit tests for the header-only cppini::IniDocument.
 *
 * Self-contained: a tiny CHECK macro counts failures and returns non-zero from
 * main() if any assertion fails, so it slots into CTest without a framework.
 */

#include "cppini/IniParser.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int g_failures = 0;
int g_checks   = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                 \
    } while (0)

constexpr const char* kSample =
    "; global config\n"
    "app_name = CPPIniParser\n"
    "version  = 1.0\n"
    "[network]\n"
    "host = \"example.com\"   ; inline comment\n"
    "port = 8080\n"
    "ratio = 0.25\n"
    "mask = 0xFF\n"
    "timeout : 30\n"            // ':' separator
    "proxy =\n"                 // empty value
    "[server]\n"
    "enabled = yes\n"
    "password = 'p@ss #word ; not-a-comment'\n";

void test_load_and_get() {
    cppini::IniDocument doc;
    CHECK(doc.loadString(kSample) == cppini::Status::Ok);

    // Strings, including the global ("") section.
    CHECK(doc.getString("", "app_name") == "CPPIniParser");
    CHECK(doc.getString("network", "host") == "example.com");
    // Quotes preserve comment characters and spaces.
    CHECK(doc.getString("server", "password") == "p@ss #word ; not-a-comment");
    // Empty value parses to an empty string.
    CHECK(doc.getString("network", "proxy", "x") == "");

    // Integers: decimal + hex, and ':' separator.
    CHECK(doc.get<int>("network", "port", -1) == 8080);
    CHECK(doc.get<long>("network", "mask", -1) == 255);
    CHECK(doc.getInt("network", "timeout", -1) == 30);

    // Double.
    CHECK(std::fabs(doc.get<double>("network", "ratio", 0.0) - 0.25) < 1e-9);

    // Bool.
    CHECK(doc.get<bool>("server", "enabled", false) == true);

    // Fallbacks for missing keys/sections.
    CHECK(doc.get<int>("network", "missing", 42) == 42);
    CHECK(doc.getString("nope", "x", "def") == "def");
    CHECK(doc.has("network", "host"));
    CHECK(!doc.has("network", "nope"));

    // Optional access.
    CHECK(doc.getOptional("network", "host").has_value());
    CHECK(!doc.getOptional("network", "nope").has_value());
}

void test_modify() {
    cppini::IniDocument doc;
    CHECK(doc.loadString(kSample) == cppini::Status::Ok);
    const std::size_t before = doc.size();

    // Overwrite existing — count unchanged.
    doc.setValue("network", "port", 9090);
    CHECK(doc.get<int>("network", "port", 0) == 9090);
    CHECK(doc.size() == before);

    // Typed setter for bool.
    doc.setValue("server", "enabled", false);
    CHECK(doc.get<bool>("server", "enabled", true) == false);

    // Floating-point setter round-trips.
    doc.setValue("network", "ratio", 2.5);
    CHECK(std::fabs(doc.get<double>("network", "ratio", 0.0) - 2.5) < 1e-9);
}

void test_add() {
    cppini::IniDocument doc;
    CHECK(doc.loadString(kSample) == cppini::Status::Ok);
    const std::size_t before = doc.size();

    doc.set("network", "gateway", "10.0.0.1");
    CHECK(doc.getString("network", "gateway") == "10.0.0.1");

    doc.setValue("limits", "max", 100);  // brand-new section
    CHECK(doc.get<int>("limits", "max", 0) == 100);

    CHECK(doc.size() == before + 2);
}

void test_remove() {
    cppini::IniDocument doc;
    CHECK(doc.loadString(kSample) == cppini::Status::Ok);

    CHECK(doc.remove("network", "port") == cppini::Status::Ok);
    CHECK(!doc.has("network", "port"));
    CHECK(doc.has("network", "host"));  // sibling survives
    CHECK(doc.remove("network", "port") == cppini::Status::NotFound);
}

void test_errors() {
    cppini::IniDocument doc;
    CHECK(doc.loadString("[unterminated\n") == cppini::Status::SyntaxError);
    doc.clear();
    CHECK(doc.loadString("novalue\n") == cppini::Status::SyntaxError);
}

void test_line_endings() {
    cppini::IniDocument doc;
    CHECK(doc.loadString("[s]\r\na = 1\rb = 2\nc = 3") == cppini::Status::Ok);
    CHECK(doc.get<int>("s", "a", 0) == 1);
    CHECK(doc.get<int>("s", "b", 0) == 2);
    CHECK(doc.get<int>("s", "c", 0) == 3);
}

void test_escapes() {
    cppini::IniDocument doc;
    CHECK(doc.loadString("k = \"line1\\nline2\\t!\"\n") == cppini::Status::Ok);
    CHECK(doc.getString("", "k") == "line1\nline2\t!");
}

void test_dump_roundtrip() {
    cppini::IniDocument doc;
    CHECK(doc.loadString(kSample) == cppini::Status::Ok);

    cppini::IniDocument reparsed;
    CHECK(reparsed.loadString(doc.dump()) == cppini::Status::Ok);
    CHECK(reparsed.getString("network", "host") == "example.com");
    CHECK(reparsed.get<int>("network", "port", 0) == 8080);
    CHECK(reparsed.size() == doc.size());
}

void test_merge() {
    cppini::IniDocument doc;
    CHECK(doc.loadString("[a]\nx = 1\n") == cppini::Status::Ok);
    CHECK(doc.loadString("[a]\nx = 2\ny = 3\n") == cppini::Status::Ok);
    CHECK(doc.get<int>("a", "x", 0) == 2);  // overwritten
    CHECK(doc.get<int>("a", "y", 0) == 3);  // appended
}

}  // namespace

int main() {
    test_load_and_get();
    test_modify();
    test_add();
    test_remove();
    test_errors();
    test_line_endings();
    test_escapes();
    test_dump_roundtrip();
    test_merge();

    if (g_failures == 0) {
        std::printf("all %d checks passed\n", g_checks);
        return 0;
    }
    std::printf("%d/%d checks FAILED\n", g_failures, g_checks);
    return 1;
}
