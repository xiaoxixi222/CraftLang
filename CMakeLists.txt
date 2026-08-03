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
# libclang (LLVM/Clang 的 C 接口)
if(DEFINED ENV{LIBCLANG_PATH})
    set(LIBCLANG_ROOT "$ENV{LIBCLANG_PATH}")
else()
    set(LIBCLANG_ROOT "" CACHE PATH "libclang 安装路径（含 include/ 和 lib/）")
endif()

if(NOT LIBCLANG_ROOT)
    message(FATAL_ERROR "LIBCLANG_PATH 环境变量未设置或路径无效")
endif()

# 统一为正斜杠，避免反斜杠转义和空格导致链接失败
file(TO_CMAKE_PATH "${LIBCLANG_ROOT}" LIBCLANG_ROOT)

target_include_directories(craftlang PRIVATE "${LIBCLANG_ROOT}/include")

# 注入项目 include 绝对路径，供 libclang 运行期解析用户源码时定位 <mcstd.h> 等头文件
target_compile_definitions(craftlang PRIVATE
    "CRAFTLANG_SRC_INCLUDE=\"${CMAKE_SOURCE_DIR}/include\""
)

# Windows 用 libclang.lib（MinGW 下即 libclang.dll.a 的导入库）
if(WIN32)
    target_link_libraries(craftlang PRIVATE "${LIBCLANG_ROOT}/lib/libclang.lib")
else()
    target_link_libraries(craftlang PRIVATE "${LIBCLANG_ROOT}/lib/libclang.so")
endif()

# 构建后将 libclang.dll 复制到可执行文件旁
if(WIN32)
    add_custom_command(TARGET craftlang POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LIBCLANG_ROOT}/bin/libclang.dll"
            "$<TARGET_FILE_DIR:craftlang>"
    )
endif()

message(STATUS "libclang found at ${LIBCLANG_ROOT}")

# ── Compiler flags ──────────────────────────────────────────────────
if(MSVC)
    target_compile_options(craftlang PRIVATE /W4)
else()
    target_compile_options(craftlang PRIVATE -Wall -Wextra)
endif()
