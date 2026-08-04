# CraftLang

将类 C 语法的源文件编译为 Minecraft Java 版 1.20.5+ 数据包（`.mcfunction`）的编译器。前端基于 **libclang** 解析源码生成 AST，后端用 C++ 生成计分板（scoreboard）、存储（storage）与宏函数（`/function ... with`）形式的数据包。架构设计见 `design.md`。

## 当前进度（V1.0 阶段）

已实现：

- libclang 解析源码、AST 采集与递归打印（`lang.h`/`lang.cpp`）
- 读取 `setting.json`（数据包名、`pack_format`、描述）
- 输出目录清理与 `pack.mcmeta` 生成
- 函数注册表 `Function`（函数名、返回类型、参数、编号、游标）
- 数据包骨架：
  - `start.mcfunction`：初始化 `functionSpace` 计分板并宏调用 `main`
  - 每个带函数体的函数生成 `<编号>.mcfunction`，内含参数绑定宏指令
- 内建函数保护：`print_int` 等若有函数体则报错终止编译

尚未实现（后续版本）：

- 函数体内语句/表达式代码生成（`DeclStmt`、`ReturnStmt`、调用等）
- 局部变量、四则运算、函数调用传参、返回值、递归

## 目录结构

```
include/craftlang/
  lang.h         AST 采集/遍历层：all_cursors、CursorHash、回调、递归打印
  compiler.h     代码生成层：Parm、Function 结构体、compile()
  config.h       Config：name / pack_format / description
include/mcstd.h  内建函数声明（print_int）
src/
  main.cpp       入口：读配置、libclang 解析、打印 AST、调用 compile()
  lang.cpp       AST 采集与遍历实现
  compiler.cpp   代码生成实现（deal_cursor、compile、符号表）
example/
  test.c         示例源码
  setting.json   示例配置
third_party/json/json.hpp   nlohmann/json 单头文件
design.md        六阶段架构演进设计
```

## 依赖与构建

- CMake >= 3.15
- 支持 C++17 的编译器（本项目用 llvm-mingw 的 clang）
- libclang（LLVM/Clang C 接口）
- nlohmann/json（已随项目提供）

```powershell
# 1. 设置 libclang 路径（按你的安装位置）
$env:LIBCLANG_PATH = "C:\Program Files\LLVM"

# 2. 配置并构建
cmake -G "MinGW Makefiles" -B build
cmake --build build
```

> CMake 用 `file(GLOB_RECURSE)` 收集源文件，**新增/删除 `src/*.cpp` 后需重新运行配置命令**。

## 使用

```powershell
build\craftlang.exe example\test.c example\setting.json
```

运行后会：

1. 用 libclang 解析源码并打印 AST
2. 若 libclang 有任何诊断即抛异常终止（见已知限制）
3. 清空并重建 `build\<数据包名>\`
4. 生成 `pack.mcmeta` 与 `data\<数据包名>\function\` 下的 `.mcfunction` 文件

`_DEBUG` 编译时额外打印节点收集过程与构建路径信息。

### 配置（setting.json）

```json
{
    "name": "craftlang",
    "pack_format": 101,
    "description": "test"
}
```

### 输出示例（example/test.c）

```
build/craftlang/
├── pack.mcmeta
└── data/craftlang/function/
    ├── start.mcfunction      初始化 functionSpace 并宏调用 main
    ├── 1.mcfunction          add 的参数绑定
    └── 2.mcfunction          main 的参数绑定
```

当前阶段函数体内语句尚未生成；`.mcfunction` 内容仅为参数绑定宏指令。

## 已知限制（当前实现）

1. **函数体内语句不生成代码**：`DeclStmt`、`ReturnStmt`、表达式、函数调用等 case 均为空，生成的 `.mcfunction` 只含参数绑定，没有实际逻辑。
2. **函数编号按 AST 遍历顺序分配**：`functionCounter` 按遇到 `FunctionDecl` 的先后递增，无函数体的声明和头文件中的内建声明（如 `print_int`）也占编号，文件名为纯编号（如 `1.mcfunction`）。因此 `main` 的编号不是固定的 0，而是它前面所有 `FunctionDecl` 的数量——新增/重排函数会改变 `start.mcfunction` 里对 `main` 的引用。
3. **重复定义由 libclang 兜底**：真正重复定义（两个函数体）libclang 会报 `redefinition` 诊断，命中"任何诊断即终止"而停止编译；`functionMap` 的 find 守卫仅用于「前向声明 + 后续定义」这类合法情况，避免重复登记编号。
4. **任何 libclang 诊断都会终止编译**：`diagCount > 0` 即抛 `parse error`（警告也算），源码需保证零警告。
5. **参数宏槽无数据来源**：生成的 `$(0)`、`$(1)` 等参数宏依赖调用方 `/function ... with` 传入，而 `start.mcfunction` 的 storage 目前只写了 `functionSpace`，未写入参数槽——参数绑定链路尚未闭环。
6. **宏栈帧机制未实现**：`functionSpace` 只初始化为 0，无调用前压栈、返回弹栈逻辑；所有函数共享同一 `functionSpace`，暂不支持递归/嵌套调用。
7. **全局变量生成未实现**：如 `int d = 0;` 目前不生成指令；设计上归入 "Global" 函数空间（`deal_cursor` 在 TranslationUnit 处将 `current_function` 置为 Global，即为此预留），属尚未实现。
8. **函数返回值无传递机制**：没有 return 计分板约定，`return` 语句不生成代码。
9. **未区分局部变量与全局变量**：`localVarsToInt` 目前只登记当前函数参数；全局变量尚未接入（设计上归 "Global" 函数空间），待实现。
10. **类型未参与代码生成**：`Parm` 虽采集了参数 `CXType`，但代码生成不区分类型，仅支持 int 语义，无类型检查。
11. **无 main 时 start 只初始化**：`start.mcfunction` 仅在有 `main` 注册时追加调用指令。
12. **输出命名不可读**：函数文件名与 C 函数名不直接对应，仅靠 `functionMap` 内部映射，调试不便。