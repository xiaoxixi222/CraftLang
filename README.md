# CraftLang

将类 C 语法的源文件编译为 Minecraft Java 版数据包（`.mcfunction`）的编译器。

## 依赖

- **编译器**：GCC / Clang（支持 C++17）
- **CMake** >= 3.15
- **libclang**（LLVM/Clang C 接口，用于解析并生成 AST）
- **nlohmann/json**：`third_party/json/json.hpp`(暂未使用)

## 构建
首先，你需要安装libclang依赖,然后设置环境变量LIBCLANG_PATH,或配置CMake的LibClang_ROOT选项。

```bash
# 配置（如 libclang 未自动找到，追加 -DLibClang_ROOT=<路径>）
cmake -G "MinGW Makefiles" -B build

# 编译
cmake --build build
```

## 使用

```bash
./build/craftlang example/test.c
```
