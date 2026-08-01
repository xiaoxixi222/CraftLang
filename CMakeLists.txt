cmake_minimum_required(VERSION 3.15)
project(CraftLang VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Source files ────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# ── Executable target (must be defined before target_* calls) ──────
add_executable(craftlang ${SOURCES})

# ── Include directories ────────────────────────────────────────────
target_include_directories(craftlang PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

# ── Third-party dependencies ────────────────────────────────────────
# nlohmann/json (header-only)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/json/json.hpp")
    target_include_directories(craftlang PRIVATE "${CMAKE_SOURCE_DIR}/third_party/json")
    message(STATUS "nlohmann/json found in third_party/json")
else()
    message(WARNING "nlohmann/json.hpp not found in third_party/json/")
endif()

# libclang (LLVM/Clang 的 C 接口) — 通过 find_package 或 -DLibClang_ROOT 指定
find_package(libclang CONFIG QUIET)
if(libclang_FOUND)
    target_link_libraries(craftlang PRIVATE libclang)
    message(STATUS "libclang found via find_package")
else()
    message(WARNING "libclang not found; set -DLibClang_ROOT=<path> or install libclang")
endif()

# ── Compiler flags ──────────────────────────────────────────────────
if(MSVC)
    target_compile_options(craftlang PRIVATE /W4)
else()
    target_compile_options(craftlang PRIVATE -Wall -Wextra)
endif()
