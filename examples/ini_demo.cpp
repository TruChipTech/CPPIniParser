/**
 * @file ini_demo.cpp
 * @brief Sample application built on the header-only cppini::IniDocument.
 *
 * Demonstrates the full workflow: load a file, read typed values, modify and
 * add entries, then serialize the result back to INI text.
 *
 * @code
 *   ini_demo examples/sample.ini
 * @endcode
 */

#include "cppini/IniParser.hpp"

#include <iostream>
#include <string>

/// A typical application config populated straight from an INI document.
struct AppConfig {
    std::string host;
    int         port    = 0;
    int         timeout = 0;
    double      ratio   = 0.0;
    bool        enabled = false;
};

/// Map INI entries onto a strongly-typed config struct.
static AppConfig loadConfig(const cppini::IniDocument& doc) {
    AppConfig c;
    c.host    = doc.get<std::string>("network", "host", "localhost");
    c.port    = doc.get<int>("network", "port", 8080);
    c.timeout = doc.get<int>("network", "timeout", -1);
    c.ratio   = doc.get<double>("network", "ratio", 1.0);
    c.enabled = doc.get<bool>("server", "enabled", false);
    return c;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <file.ini>\n";
        return 2;
    }

    cppini::IniDocument doc;
    cppini::Status rc = doc.loadFile(argv[1]);
    if (rc != cppini::Status::Ok) {
        std::cerr << "load error: " << cppini::toString(rc) << '\n';
        return 1;
    }

    std::cout << "CPPIniParser " << cppini::kVersion << " — loaded "
              << doc.size() << " entries\n";

    AppConfig cfg = loadConfig(doc);
    std::cout << "--- typed config ---\n"
              << "host    = " << cfg.host    << '\n'
              << "port    = " << cfg.port    << '\n'
              << "timeout = " << cfg.timeout << '\n'
              << "ratio   = " << cfg.ratio   << '\n'
              << "enabled = " << std::boolalpha << cfg.enabled << '\n';

    // Modify an existing field and add a new one.
    doc.setValue("network", "port", 9090);
    doc.set("network", "gateway", "10.0.0.1");

    std::cout << "--- after modify/add ---\n"
              << "port    = " << doc.get<int>("network", "port", 0) << '\n'
              << "gateway = " << doc.getString("network", "gateway") << '\n'
              << "entries = " << doc.size() << "\n\n"
              << "--- serialized ---\n"
              << doc.dump();
    return 0;
}
