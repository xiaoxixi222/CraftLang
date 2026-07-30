cmake_minimum_required(VERSION 3.15)
project(CraftLang VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# ── Library & package search paths ──────────────────────────────────
link_directories(
    "${CMAKE_SOURCE_DIR}/lib"
    "${CMAKE_SOURCE_DIR}/packages"
)

file(GLOB_RECURSE EXTERNAL_LIBS
    "${CMAKE_SOURCE_DIR}/lib/*.lib"
    "${CMAKE_SOURCE_DIR}/lib/*.a"
    "${CMAKE_SOURCE_DIR}/packages/*.lib"
    "${CMAKE_SOURCE_DIR}/packages/*.a"
)

if(EXTERNAL_LIBS)
    target_link_libraries(craftlang PRIVATE ${EXTERNAL_LIBS})
    message(STATUS "Linking external libraries: ${EXTERNAL_LIBS}")
endif()

# ── Third-party dependencies ────────────────────────────────────────
# nlohmann/json (header-only)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/json/json.hpp")
    target_include_directories(craftlang PRIVATE "${CMAKE_SOURCE_DIR}/third_party/json")
    message(STATUS "nlohmann/json found in third_party/json")
else()
    message(WARNING "nlohmann/json.hpp not found in third_party/json/")
endif()

# CppParser (vendored via add_subdirectory)
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/cpp-parser/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/third_party/cpp-parser")
    target_link_libraries(craftlang PRIVATE cppparser)
    message(STATUS "CppParser found in third_party/cpp-parser")
else()
    message(WARNING "CppParser not found in third_party/cpp-parser/")
endif()

# ── Compiler flags ──────────────────────────────────────────────────
if(MSVC)
    target_compile_options(craftlang PRIVATE /W4)
else()
    target_compile_options(craftlang PRIVATE -Wall -Wextra)
endif()
