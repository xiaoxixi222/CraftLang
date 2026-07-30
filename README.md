# CraftLang

将类 C 语法的源文件编译为 Minecraft Java 版数据包（`.mcfunction`）的编译器。

## 依赖

- **编译器**：GCC / Clang（支持 C++17）
- **CMake** >= 3.15
- **Flex**（CppParser 依赖）
- **CppParser**：`third_party/cpp-parser/`（`git submodule update --init`）
- **nlohmann/json**：`third_party/json/json.hpp`

## 构建

```bash
# 拉取第三方依赖（如未克隆）
git submodule update --init --recursive

# 配置
cmake -B build

# 编译
cmake --build build
```

## 使用

```bash
./build/craftlang test.cl
```
