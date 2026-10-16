# CraftLang

将类 C 语法的源文件编译为 Minecraft Java 版 1.20.5+ 数据包（`.mcfunction`）的工具链，由三个可执行程序组成：

| 程序 | 作用 | 状态 |
|---|---|---|
| `craftlang` | 编译器：libclang 解析源码，生成 `.mcfunction` 与符号表 JSON | 基本可用 |
| `craftlinker` | 链接器：合并多个编译单元的符号表、统一编号、产出完整数据包 | 基本可用 |
| `craftsim` | 执行模拟器：读取数据包、展开宏函数、模拟执行 | 开发中 |

前端基于 **libclang** 解析源码生成 AST，后端用 C++ 生成计分板（scoreboard）、存储（storage）与宏函数（`/function ... with`）形式的 mcfunction。架构与 V1.0~V6.0 版本规划见 `design.md`。

## 工作流程

```
test.c ──craftlang──▶ build/test_o/function/*.mcfunction
                      build/test_symbols.json        （每个源文件各产出一份）
                          │
                          ▼
        ──craftlinker──▶ build/<name>/<name>/pack.mcmeta
                         build/<name>/<name>/data/<name>/function/*.mcfunction
                         build/<name>/<name>/data/minecraft/tags/function/load.json
                          │
                          ▼
        ──craftsim────▶ 模拟执行（开发中）
```

编译阶段不决定最终编号，只在文件内留下占位符（`.f.N.f.` 函数、`.g.N.g.` 全局变量、`..name..` 数据包名），由链接器统一替换成跨文件的最终编号。

## 运行时机制

- **计分板 = 变量**：每个 int 变量对应一个计分板 objective，玩家名固定为数据包名。
- **调用 = 宏函数 + storage 传参**：调用方 `execute store result storage <name> 0 int 1 ...` 写入第 N 个参数，再 `function <name>:<f> with storage <name>`，被调方用 `$(0)`、`$(1)` 读取。
- **栈帧 = `functionSpace` 计分板**：局部变量命名 `$(functionSpace)_tmp_0`，调用前 `functionSpace` 自增、返回后自减，实现动态栈帧寻址。
- **控制流 = `ifFlag` / `interrupted` 中断协议**：`if/else` 拆成子函数，用 `ifFlag` 决定跳转；`return` 置 `interrupted = 2` 并 `return 0` 逐层退出。
- **宏展开 = `dueWithFunction`**：见已知限制 7。

## 当前进度

### 已实现

**编译器（`craftlang`）**

- libclang 解析源码、AST 采集与递归打印（`lang.h`/`lang.cpp`）
- 语句：函数定义/声明、`DeclStmt`、`ReturnStmt`、`if/else`、复合语句、表达式语句
- 表达式：四则运算、取模、比较运算符、复合赋值（`+=` 等）、一元运算、函数调用、括号
- 变量：全局变量、局部变量、块级作用域（同名变量按作用域链由内向外解析）
- `extern` 声明采集，跨文件符号由链接器解决
- 符号表输出 `<文件名>_symbols.json`（`Global` + `function` 两类，含 `isExtern`/`number`）
- 内建函数保护：`print_int` 等若有函数体则报错终止编译

**链接器（`craftlinker`）**

- 读取 `setting.json`（数据包名、`pack_format`、描述、待链接文件、输出路径）
- 符号收集与统一编号，`extern` 未找到定义时报错终止（`undefined extern function/global`）
- `.f.N.f.` / `.g.N.g.` / `..name..` 占位符全局替换
- 生成 `pack.mcmeta`、`start.mcfunction`、`data/minecraft/tags/function/load.json`
- `other/` 目录资源（如 load.json）拷贝并替换占位符

**内建库（`mcstd/`）**

- `print_int` 实现为 `$say $(0)`，含符号表与 `load.json`，随链接器一起接入

**模拟器（`craftsim`）**

- NBT 数据模型与 13 种标签类型、`Storage`/`Scoreboard`（`storage.hpp`/`storage.cpp`）
- NBT 路径 AST（`PathNode`：Key/Index/Wildcard/Filter）与 `NbtPathEvaluator` 求值器
- mcfunction 分词器：word/string/newline/end/error token（`function.cpp`）
- 宏展开 `dueWithFunction`：纯字符串变换，不执行任何指令
- `executeFunction` 骨架、`loadDatapack` 读取 `pack.mcmeta` 与目录列表

### 尚未实现

- `while` / `do-while` / `for` 循环（编译时抛 `unsupported cursor: WhileStmt`）
- 除 `int` 外的类型（浮点、字符、数组、结构体、指针均报 `unsupported type`）
- craftsim 的实际指令执行与单步调试
- `loadDatapack` 尚未把 `.mcfunction` 注册进 `function_map`（见已知限制 5）

## 目录结构

```
include/
  craftlang/
    lang.h         AST 采集/遍历层：all_cursors、CursorHash、回调、递归打印
    compiler.h     代码生成层：Parm/Function/Var、compile()、表达式与语句处理
    config.h       Config：name / pack_format / description / output_path
  linker/
    linker.h       符号收集、占位符替换、link()
    config.h       链接器配置：name / linkFile / outputPath
  sim/
    sim.hpp        extern Storage/Scoreboard 全局实例声明
    storage.hpp    NBT 类型、NbtPath AST、NbtPathEvaluator、Storage、Scoreboard
    function.hpp   Function 结构、分词器、dueWithFunction、executeFunction
    datapack.hpp   function_map、loadDatapack
  fsUtils.hpp      文件系统工具（读写、目录、复制重命名等）
  mcstd.h          内建函数声明（print_int）
src/
  compiler/        lang.cpp、compiler.cpp、main.cpp
  linker/          linker.cpp、main.cpp
  sim/             storage.cpp、function.cpp、datapack.cpp、main.cpp
mcstd/             预编译内建库：mcstd_symbols.json + mcstd_o/（function、other）
example/
  test*.c          示例源码（test、test2、test_scope、test_while、test_for）
  setting.json     链接器配置示例
  test/            手写数据包示例（供 craftsim 读取）
third_party/json/json.hpp   nlohmann/json 单头文件
design.md          六阶段架构演进设计
```

## 依赖与构建

- CMake >= 3.15
- 支持 C++17 的编译器（`craftsim` 目标要求 C++23；本项目用 llvm-mingw 的 clang）
- libclang（LLVM/Clang C 接口）
- nlohmann/json（已随项目提供）

```powershell
# 1. 设置 libclang 路径（按你的安装位置）
$env:LIBCLANG_PATH = "C:\Program Files\LLVM"

# 2. 配置并构建
cmake -G "MinGW Makefiles" -B build
cmake --build build                      # 全部目标
cmake --build build --target craftlang   # 只构建某一个
```

> CMake 用 `file(GLOB_RECURSE)` 收集源文件，**新增/删除 `src/*.cpp` 后需重新运行配置命令**。

构建产物：`build/craftlang.exe`、`build/craftlinker.exe`、`build/craftsim.exe`。

## 使用

### 1. 编译单个源文件

```powershell
build\craftlang.exe <源文件.c> [输出目录，默认当前目录下的 build\]
```

例如：

```powershell
build\craftlang.exe example\test_scope.c build
```

运行后会：

1. 用 libclang 解析源码并打印 AST（`_DEBUG` 编译时更详细）
2. 若 libclang 有任何诊断即抛异常终止（见已知限制 4）
3. 清空并重建 `<输出目录>\<文件名>_o\function\`
4. 生成 `<输出目录>\<文件名>_symbols.json`

### 2. 链接成数据包

```powershell
build\craftlinker.exe example\setting.json
```

链接前需先编译 `setting.json` 的 `linkFile` 里列出的每个源文件，使对应 `_symbols.json` 与同名 `_o` 目录就位。

### 3. 模拟执行（开发中）

```powershell
build\craftsim.exe
```

目前 `main.cpp` 里硬编码了 `loadDatapack` + `executeFunction` 调用，仅用于联调。

## 配置（链接器 setting.json）

```json
{
    "name": "craftlang",
    "pack_format": 101,
    "description": "test",
    "linkFile": [
        "build\\test_symbols.json",
        "build\\test2_symbols.json",
        "build\\test_scope_symbols.json",
        "mcstd\\mcstd_symbols.json"
    ],
    "outputPath": "build\\craftlang"
}
```

链接器按每个 `_symbols.json` 的路径推导出同名 `_o` 目录（`test_symbols.json` → `test_o\`），把其中的 `.mcfunction` 重编号后写入 `<outputPath>\<name>\<name>\data\<name>\function\`。

## 输出示例

以 `example/test.c` + `test2.c` + `test_scope.c` 链接为例：

```
build/craftlang/craftlang/
├── pack.mcmeta
└── data/
    ├── craftlang/function/
    │   ├── start.mcfunction    初始化 functionSpace/tmp/ifFlag/interrupted、全局变量，宏调用 main
    │   ├── 0.mcfunction        main
    │   ├── 1.mcfunction        scope_test
    │   ├── 1_0.mcfunction      scope_test 内 if 分支体
    │   └── 2.mcfunction        print_int（$say $(0)）
    └── minecraft/tags/function/
        └── load.json           注册 craftlang:start
```

## 已知限制（当前实现）

1. **仅支持 `int` 类型**：`initVar`/`setVarToVar` 只接受 `CXType_Int`，其余类型报 `unsupported type` 并终止；浮点、字符、数组、结构体、指针留待 V2.0+。
2. **不支持循环语句**：`while`/`do-while`/`for` 命中 `deal_cursor` 的 default 分支，抛 `unsupported cursor: WhileStmt` 终止编译。
3. **函数编号由 AST 遍历顺序决定**：文件名为纯编号，`main` 的编号取决于它前面的 `FunctionDecl` 数量；链接器会按符号表统一重编号，但新增/重排函数仍会改变占位符对应关系。
4. **任何 libclang 诊断都会终止编译**：`diagCount > 0` 即抛 `parse error`（警告也算），源码需保证零警告。
5. **craftsim 尚不能链接运行**：`datapack.hpp` 中 `function_map` 只有 `extern` 声明、`loadDatapack` 尚未实现注册逻辑，导致未定义符号链接错误；`executeFunction` 目前只做宏展开并 `_DEBUG` 打印，不执行任何指令。
6. **分词器尚未接入**：`initTokenizeContext`/`tokenize` 已实现但没有调用方，等待与执行链路对接。
7. **`dueWithFunction` 的宏展开规则**：只有行首 `$` 的行会被处理（去掉该行开头的 `$` 后展开）；无 `$(...)` 时原样返回，不依赖 `source`；`source` 查不到、或某 key 不在宏表中时保留 `$(key)` 原样；宏表值只接受 String 与数值类型，List/Compound/数组跳过。

## 许可

见 [LICENSE](LICENSE)。
