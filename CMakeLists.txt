cmake_minimum_required(VERSION 3.14)

project(CPPIniParser
        VERSION 1.0.0
        DESCRIPTION "Single-header, C++17 templated INI parser"
        LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Header-only INTERFACE library. Consumers just link `cppini::cppini` and get
# the include path; there is no .a/.so to build or install.
# ---------------------------------------------------------------------------
add_library(cppini INTERFACE)
add_library(cppini::cppini ALIAS cppini)

target_include_directories(cppini INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(cppini INTERFACE cxx_std_17)

# Size/performance: default to an optimized, size-conscious release build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(CPPINI_BUILD_EXAMPLES "Build the sample application" ON)
option(CPPINI_BUILD_TESTS    "Build the unit tests"        ON)

if(CPPINI_BUILD_EXAMPLES)
    add_executable(ini_demo examples/ini_demo.cpp)
    target_link_libraries(ini_demo PRIVATE cppini::cppini)
    target_compile_options(ini_demo PRIVATE -Wall -Wextra)
endif()

if(CPPINI_BUILD_TESTS)
    enable_testing()
    add_executable(test_ini tests/test_ini.cpp)
    target_link_libraries(test_ini PRIVATE cppini::cppini)
    target_compile_options(test_ini PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME unit_tests COMMAND test_ini)
endif()

# ---------------------------------------------------------------------------
# Install the headers (header-only: nothing else to ship).
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
