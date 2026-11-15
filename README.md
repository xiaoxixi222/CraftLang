# CraftLang

将类 C 语法的源文件编译为 Minecraft Java 版 1.20.5+ 数据包（`.mcfunction`）的工具链，由三个可执行程序组成：

| 程序 | 作用 | 状态 |
|---|---|---|
| `craftlang` | 编译器：libclang 解析源码，生成 `.mcfunction` 与符号表 JSON | 基本可用（仅 int；支持 if/else，无循环） |
| `craftlinker` | 链接器：合并多个编译单元的符号表、统一编号、产出完整数据包 | 基本可用 |
| `craftsim` | 执行模拟器：读取数据包、展开宏函数、逐条解释 mcfunction | 部分可用（`say`/`return`/`scoreboard`/`execute`/`function`） |

前端基于 **libclang** 解析源码生成 AST，后端用 C++ 直接生成计分板（scoreboard）、存储（storage）与宏函数（`/function ... with`）形式的 mcfunction。架构与 V1.0~V6.0 版本规划见 `design.md`；当前实现状态以本文为准。

## 工作流程

### 目标链路（Minecraft 实际运行）

```
test.c ──craftlang──▶ build/test_o/function/
                        ├── start.mcfunction          全局变量初始化
                        ├── .f.N.f..mcfunction        函数体
                        └── .f.N.f._M.mcfunction      if/else 分支体
                      build/test_symbols.json          每个源文件各一份
                          │
                          ▼
        ──craftlinker──▶ build/<name>/<name>/
                          ├── pack.mcmeta
                          ├── data/<name>/function/
                          │   ├── start.mcfunction    运行时初始化 + 各单元 start + 调用 main
                          │   ├── <N>.mcfunction
                          │   └── <N>_<M>.mcfunction
                          └── data/minecraft/tags/function/load.json
                          │
                          ▼
                     Minecraft /function 宏机制执行
```

编译阶段不决定最终编号，只在文件内留下占位符（`.f.N.f.` 函数、`.g.N.g.` 全局变量、`..name..` 数据包名），由链接器统一替换成跨文件的最终编号。

链接器分两遍：

1. 读取所有 `_symbols.json`，按符号名收集函数/全局变量，按首次出现分配编号，记录 `extern` 与定义状态；
2. 校验未定义的 `extern`，逐文件替换占位符，合并各单元的 `start.mcfunction`，拷贝 `other/` 资源，最后生成 `pack.mcmeta` 与最终 `start.mcfunction`。

### 当前 craftsim 联调链路

`craftsim` 还没有接到链接器产物上，当前入口固定读取手写数据包 `example/test`：

```
example/test/
  pack.mcmeta
  data/test/function/test.mcfunction
        │
        ▼
  loadDatapack ──▶ function_map[namespace][function]
        │
        ▼
  dueWithFunction ──▶ 宏展开后的文本
        │
        ▼
  tokenize ──▶ executeFunction 命令分派
        │
        ├── say        ──▶ output
        ├── return     ──▶ returnValue + isReturn
        ├── scoreboard ──▶ objectives / players 读写
        ├── execute    ──▶ run / if / unless / store
        ├── function   ──▶ 子函数调用 + with storage
        └── 其他       ──▶ Unknown command 错误
        │
        ▼
  ReturnState { returnValue, successFlag, isReturn, out, error, warn, info }
```

因此，`craftsim` 已能执行 `scoreboard`、`execute`、`function` 组成的调用链；`data` 仍未实现，所以还不能覆盖编译器可能发射的全部命令。

## 数据包运行时机制（Minecraft 侧）

- **计分板 = 变量**：每个变量对应一个 objective，objective 名就是变量名；所有分数都挂在“数据包名”这个假玩家上。全局变量在编译单元内命名为 `0_.g.N.g.`（链接后为 `0_<最终编号>`），函数局部变量命名为 `$(functionSpace)_N`，表达式临时量命名为 `$(functionSpace)_tmp_N`。
- **运行时 objective**：`start.mcfunction` 创建 `functionSpace`、`tmp`、`ifFlag`、`interrupted`，并把 `functionSpace` 设为 1、`interrupted` 设为 0。比较运算还会使用 `tmp` 作为临时槽，单目负号使用 `const-1`。
- **调用 = 宏函数 + storage 传参**：调用方先把第 i 个参数写入 `storage <name> <i>`（`execute store result storage ... int 1`），再把当前 `functionSpace` 写入 `storage <name> functionSpace`，最后执行 `function <name>:<f> with storage <name>`；被调方通过 `$(0)`、`$(1)` 读取参数，通过 `$(functionSpace)` 寻址自己的栈帧。
- **栈帧 = `functionSpace`**：调用前 `functionSpace` 自增，调用后自减；每次调用都有独立的局部变量/临时量 objective。函数入口会用 `initVar` 重新创建参数 objective，并从 `$(i)` 初始化。
- **控制流 = `ifFlag` + 分支函数**：`if/else` 的 then/else 体会编译成独立的 `.f.<父函数>.f._<序号>` 文件。调用方先把 `ifFlag` 置 0，执行 then 分支；then 分支正常结束时把 `ifFlag` 置 1；随后仅当 `ifFlag == 0` 时执行 else 分支。分支函数通过 storage 继承父函数的 `functionSpace`。
- **return = `interrupted` 协议**：`return <expr>` 把结果写入 `return` objective，置 `interrupted = 2` 并 `return 0` 退出当前 mcfunction。if/else 子函数返回后，父函数检查 `interrupted`，非 0 时继续 `return 0` 向上退出；普通函数调用点会在调用后把 `interrupted = 2` 清零，因此被调函数 return 不会错误地退出调用者。当前只使用 0（正常）和 2（return），1/3 保留。
- **宏行与占位符**：编译器用 `addDollarPrefix` 给所有含 `$` 的行加行首 `$`，让 Minecraft 把这些行当作宏行；链接器负责把 `.f.N.f.`、`.g.N.g.`、`..name..` 替换成最终名字。真正的宏展开由 Minecraft 的 `/function ... with storage` 完成。

## craftsim 模拟器运行时

- **数据模型**：`storage.hpp` 定义 13 种 NBT 标签、`NBT`/`NBT_List`、路径 AST（`PathNode`：Key/Index/Wildcard/Filter）、`NbtPathEvaluator`、`Storage` 与 `Scoreboard`；`scoreboard` 与 `execute store` 命令会实际读写这些状态。
- **数据包加载**：`loadDatapack` 读取 `pack.mcmeta`，扫描 `data/<namespace>/function/*.mcfunction`，把函数内容放进 `function_map[namespace][name]`；不解析 tags/`load.json`。
- **宏展开**：`dueWithFunction` 以“行首（第 0 列）`$` 且包含 `$(`”的行判定是否需要展开；没有这类行时原样返回，不要求 `source`；一旦需要展开，所有行首 `$` 行都会去掉 `$` 并替换其中的 `$(key)`。有宏但没给 `source` 时报 `Function has macros but no source provided.`；`source` 不在 storage 时报 `Source not found.`；给了 `path` 时取第一个匹配的 Compound，否则把根节点当 Compound；宏值只接受 String/Byte/Short/Int/Long/Float/Double，List/Compound/数组/End 跳过；未知 key 保留 `$(key)` 原样。
- **分词器**：`initTokenizeContext`/`tokenize` 支持 `TOKENIZE_NORMAL` 与 `TOKENIZE_LINE`；CRLF 只产生一个换行 token；token 类型为 `TOK_WORD`/`TOK_STRING`/`TOK_NEWLINE`/`TOK_END`/`TOK_ERROR`。
- **执行循环**：`executeFunction` 先做宏展开，再逐 token 查 `commandToFunction` 分发表；handler 负责消费当前行并把 token 停在 `TOK_NEWLINE`/`TOK_END`；输出中 `out` 是字符串，`error`/`warn`/`info` 是 `LogEntry{position, message}` 列表，遇到 `isReturn` 立即返回；未知命令记录为 error 并跳到行尾，`main.cpp` 按 `Error/Warn/Info: (row:col) message` 打印。
- **scoreboard**：支持 `objectives add/remove`、`players set/add/remove/get/operation`（`=` `+=` `-=` `*=` `/=` `%=`）。目标 objective 必须存在；分数缺失按 0 并记 warning；objective 缺失记 warning、不自动创建；除零/模零记 error、目标保持不变并继续执行；`players get` 通过 `ReturnState.returnValue` 返回分数。
- **execute**：支持 `run`、`if/unless score`（`=` `>` `>=` `<` `<=` 与 `matches N`/`N..M`/`..M`/`N..`）、`store result storage <ns> <key> int <scale>`。每次条件判断和 store 都记一条 info；`execute ... run return` 会把 `isReturn` 透传给父函数；不支持 `store result score`。
- **function**：支持 `function <ns>:<path>`、`with storage <ns>`、`with storage <ns> <path>`；子函数日志合并回父函数，子函数的 `return` 只终止子函数；storage 的 path 目前只支持单层 key。
- **失败语义**：除 `return` 外，命令失败只终止当前命令并继续下一行；Minecraft 有兜底语义的情况（如分数缺失按 0）记 warning，其余语法/状态错误记 error。
- **返回状态**：`ReturnState` 的 `returnValue` 是返回值（无 return 时为 0），`successFlag` 表示当前命令是否成功，`isReturn` 表示是否因 return 停止，`out` 是字符串输出，`error`/`warn`/`info` 是带位置信息的 `LogEntry` 列表。

## 当前进度

按 `design.md` 的版本划分：**V1.0 已达成；V2.0 仅完成 `if/else`，循环、受限指针、变长数组未开始；V3.0~V6.0 未开始。** 递归机制（`functionSpace` 深度计数）已就绪，但仍未实测。

### 已实现

**编译器（`craftlang`）**

- libclang 解析源码、AST 采集与递归打印；任何 libclang 诊断（warning 也算）都会抛 `parse error`
- 直接 AST → mcfunction 代码生成，没有中间 IR
- 语句：函数定义/声明、`DeclStmt`、`ReturnStmt`、`if/else`、复合语句、表达式语句
- 表达式：整数字面量、变量引用、四则运算、取模、比较运算符、复合赋值（`+=` 等）、单目 `+`/`-`、函数调用、括号
- 变量：全局变量、局部变量、块级作用域（同名变量按作用域链由内向外解析）、`extern` 声明
- 输出：`<文件名>_o/function/start.mcfunction`（全局变量初始化）、`.f.N.f..mcfunction`（函数体）、`.f.N.f._M.mcfunction`（if/else 分支）、`<文件名>_symbols.json`
- 符号表：`Global` + `function` 两类，含 `name`/`number`/`isExtern`
- `mcstd.h` 提供 `print_int` 声明及 `bool`/`true`/`false` 宏；编译器不特殊拦截用户定义的同名函数

**链接器（`craftlinker`）**

- 读取 `setting.json`（数据包名、`pack_format`、描述、待链接文件、输出路径）
- 两遍处理：先收集符号并统一编号，再校验未定义 `extern`、替换占位符、写产物
- 符号收集按 `linkFile` 顺序和符号表数组顺序首次出现分配编号；重复定义抛 `duplicate definition`
- `.f.N.f.` / `.g.N.g.` / `..name..` 全局替换
- 合并各编译单元的 `start.mcfunction`，生成 `pack.mcmeta` 与最终 `start.mcfunction`；拷贝 `other/` 下的 `load.json` 等资源
- 按 `_symbols.json` 文件名推导同名 `_o` 目录（`test_symbols.json` → `test_o\`）

**内建库（`mcstd/`）**

- `print_int` 实现为 `$say $(0)`，含 `mcstd_symbols.json`、`mcstd_o/function/.f.0.f..mcfunction`、`mcstd_o/other/minecraft/tags/function/load.json`
- `mcstd_o/function/start.mcfunction` 为空；链接时与其他单元一起合并

**模拟器（`craftsim`）**

- 13 种 NBT 标签、`NBT`/`NBT_List` 工厂与比较、`Storage`/`Scoreboard` 数据模型
- NBT 路径 AST（Key/Index/Wildcard/Filter）与 `NbtPathEvaluator`（`function ... with storage <ns> <path>` 使用单层 Key 节点）
- mcfunction 分词器：普通模式 + 行模式、CRLF、word/string/newline/end/error token
- 宏展开 `dueWithFunction`：纯字符串变换，不执行指令
- `loadDatapack`：读取 `pack.mcmeta` 与 `data/<namespace>/function` 下的函数内容
- `executeFunction`：宏展开 → 分词 → 命令分派 → 聚合输出/错误/警告/信息，遇 return 停止
- 命令分发表：`say`、`return`、`scoreboard`、`execute`、`function` 已实现；`data` 仍为 `nullptr` 占位
- `runSay`：行模式捕获整行文本，空消息报错；`runReturn`：只接受一个纯整数字面量，缺参/多参报错
- `runScoreboard`：objectives 与 players 子命令、分数读写、operation 与除零处理
- `runExecute`：run/if/unless/store、条件 info、store 写入 storage、return 透传
- `runFunction`：函数查找、`with storage` 宏参数、子函数日志合并与 return 隔离
- `main.cpp` 固定加载 `example/test` 并执行 `test:test`，用于联调

### 尚未实现

- `while` / `do-while` / `for` 循环（编译时抛 `unsupported cursor: ...`）
- 除 `int` 外的类型（浮点、字符、数组、结构体、指针均报 `unsupported type`）
- `craftsim` 的 `data` 命令；storage 路径的多层语法；`execute store result score`
- `craftsim` 的 `if/unless` 其他条件类型（block/entity/data 等），当前只支持 score
- `craftsim` 的单步调试器
- 递归的运行时实测（机制已就绪）
- V2.0 的受限指针、变长数组、循环展开、混合构建；V3.0~V6.0 全部规划

## 目录结构

```
CMakeLists.txt                三个可执行目标的 CMake 配置
.clang-tidy                   clang-tidy 检查配置
tools/
  lint.ps1                    对 src/ 运行 clang-tidy 的脚本
.opencode/
  skills/craftlang-commit/    仓库内提交检查流程（SKILL.md）
include/
  craftlang/
    lang.h         AST 采集/遍历层：all_cursors、CursorHash、回调、递归打印
    compiler.h     代码生成层：Parm/Function/Var、compile()、表达式与语句处理
    config.h       Config：name / file_name / output_path
  linker/
    linker.h       符号收集、占位符替换、link()
    config.h       链接器配置：name / pack_format / description / linkFile / outputPath
  sim/
    sim.hpp        extern Storage/Scoreboard 全局实例声明
    storage.hpp    NBT 类型、NbtPath AST、NbtPathEvaluator、Storage、Scoreboard
    function.hpp   Function/ReturnState、分词器、dueWithFunction、executeFunction、命令分发表
    datapack.hpp   function_map、loadDatapack
  fsUtils.hpp      文件系统工具（读写、目录、复制重命名等，命名空间 craftsim::fs_utils）
  mcstd.h          内建函数声明（print_int）与 bool/true/false 宏
src/
  compiler/        lang.cpp、compiler.cpp、main.cpp
  linker/          linker.cpp、main.cpp
  sim/             storage.cpp、function.cpp、datapack.cpp、main.cpp
mcstd/             预编译内建库：mcstd_symbols.json + mcstd_o/（function、other）
example/
  test.c           基础示例（全局变量、extern、函数调用）
  test2.c          跨文件 extern 示例
  test_scope.c     块级作用域示例
  test_while.c     循环目标示例（当前编译会报 unsupported cursor）
  test_for.c       循环目标示例（当前编译会报 unsupported cursor）
  setting.json     链接器配置示例
  test/            手写数据包示例（供 craftsim 读取；含 test/test2 两个命名空间）
third_party/json/json.hpp   nlohmann/json 单头文件
design.md          六阶段架构演进设计
LICENSE            许可证
```

## 工程工具

- `tools/lint.ps1`：需要 PowerShell 7 和 `build/compile_commands.json`，对 `src/` 下的翻译单元运行 `clang-tidy`；默认 warning 视为错误，失败返回 1，不自动修复。
- `.clang-tidy`：启用 `clang-diagnostic-*`、`clang-analyzer-*`、`bugprone-*`、`performance-*`、`portability-*`（少量规则关闭），关闭会大面积改写现有代码的风格类检查。

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

1. 用 libclang 解析源码并打印 AST 摘要（`_DEBUG` 编译时额外打印遍历细节）
2. 若 libclang 有任何诊断即抛异常终止（见已知限制 5）
3. 清空并重建 `<输出目录>\<文件名>_o\function\`
4. 生成 `<输出目录>\<文件名>_o\function\start.mcfunction`、函数文件与 `<输出目录>\<文件名>_symbols.json`

### 2. 链接成数据包

```powershell
build\craftlinker.exe example\setting.json
```

链接前需先编译 `setting.json` 的 `linkFile` 里列出的每个源文件，使对应 `_symbols.json` 与同名 `_o` 目录就位。`mcstd_symbols.json` 对应的 `mcstd_o` 已随仓库提供，不需要额外编译。

### 3. 模拟执行（部分可用）

```powershell
build\craftsim.exe
```

当前 `main.cpp` 固定加载 `example/test` 并执行 `test:test`。`say`、`return`、`scoreboard`、`execute`、`function` 均可用；`data` 仍会输出 `Unknown command: ...` error 并跳过该行。示例函数覆盖了计分板读写、条件判断、storage 传参、宏函数调用、除零与缺失 objective/score 等路径。运行目录需要在仓库根目录，因为数据包路径是相对路径。

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

链接器按每个 `_symbols.json` 的路径推导出同名 `_o` 目录（`test_symbols.json` → `test_o\`），把其中的 `.mcfunction` 重编号后写入 `<outputPath>\<name>\<name>\data\<name>\function\`。`linkFile` 的顺序会影响最终编号；`mcstd_symbols.json` 必须包含在内，否则 `print_int` 会以未定义 extern 报错。

## 输出示例

以 `example/test.c` + `test2.c` + `test_scope.c` 链接为例：

```
build/craftlang/craftlang/
├── pack.mcmeta
└── data/
    ├── craftlang/function/
    │   ├── start.mcfunction    运行时 objective 初始化 + 各单元全局变量初始化 + 宏调用 main
    │   ├── 0.mcfunction        main
    │   ├── 1.mcfunction        scope_test
    │   ├── 1_0.mcfunction      scope_test 内 if 分支体
    │   └── 2.mcfunction        print_int（$say $(0)）
    └── minecraft/tags/function/
        └── load.json           注册 craftlang:start
```

每个编译单元都会生成自己的 `start.mcfunction`；链接器把它们的全局变量初始化内容合并进最终 `start.mcfunction`，再在末尾调用 `main`。树中的编号仅为示例，实际编号取决于 `linkFile` 顺序与符号表顺序。

## 已知限制（当前实现）

1. **仅支持 `int` 类型**：`initVar`/`setVarToVar`/`setConstToVar` 只接受 `CXType_Int`，其余类型报 `unsupported type` 并终止；浮点、字符、数组、结构体、指针留待 V2.0+。`bool`/`true`/`false` 由 `mcstd.h` 宏映射为 int。
2. **不支持循环语句**：`while`/`do-while`/`for` 命中 `deal_cursor` 的 default 分支，抛 `unsupported cursor: ...` 终止编译；`example/test_while.c`、`test_for.c` 目前不能编译。
3. **`if` 条件只认 1**：条件表达式的结果会和 1 比较；比较运算产出 1/0，所以 `if (x > 0)` 正常，但 `if (x)` 在 `x = 2` 时不会进入分支。
4. **int 函数的空 `return`**：`return;` 不会写 `return` objective，也不会设置 `interrupted`，函数会继续执行到末尾；应写 `return <expr>;`。
5. **任何 libclang 诊断都会终止编译**：`diagCount > 0` 即抛 `parse error`（警告也算），源码需保证零警告。
6. **编号不保证稳定**：编译单元内按函数/全局变量首次出现编号；链接器按 `linkFile` 顺序和符号表数组顺序重新分配最终编号，而符号表数组由 `unordered_map` 生成，顺序不保证可移植。重排函数会改变中间文件名和占位符，但链接器按符号名映射，链接结果仍然正确。
7. **craftsim 仍未实现 `data` 命令**：`say`、`return`、`scoreboard`、`execute`、`function` 已实现，但 `data` 在分发表中为 `nullptr`，执行时输出 `Unknown command` error 并跳到行尾。`execute store result score` 与 storage 多层路径也不支持。
8. **craftsim 状态与调试能力有限**：`Storage`/`Scoreboard`/NBT 路径求值器已实现，`scoreboard` 与 `execute store` 会实际读写；`main.cpp` 固定 `example/test` 和 `test:test`，没有命令行参数或单步调试；宏展开只处理行首（第 0 列）`$` 行。
9. **`return`/`say` 参数限制**：`return` 只接受一个纯整数字面量（可带正负号），不接受表达式或计分板读取；`say` 原样捕获整行剩余文本，不解析 JSON 文本组件、选择器或转义。
10. **`dueWithFunction` 的宏展开规则**：见“craftsim 模拟器运行时”；List/Compound/数组/End 不能作为宏值，未知 key 保留 `$(key)`。

## 许可

见 [LICENSE](LICENSE)。
