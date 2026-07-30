# 项目文档：CraftLang 编译器（C++ 实现）

**项目代号**：`CraftLang`
**目标**：用 C++ 编写一个编译器，将“类 C 语法”的高级语言源文件，编译为 **纯原版（无模组）Minecraft Java版 数据包（Datapack）** 的 `.mcfunction` 命令文件及配套 `.json` 配置。

---

## 1. 输入语言规范（类 C 语法子集）

为了降低编译器复杂度并适应数据包环境，**不支持**完整的 ANSI C 标准。以下是必须支持的语法特性：

### 1.1 支持的特性
- **基本类型**：仅支持 `int` 和 `void`（`void` 仅用于无返回值函数）。
- **变量声明**：仅允许在函数开头声明（C89 风格），如 `int a; int b = 10;`。
- **运算符**：算术（`+ - * / %`）、关系（`> < >= <= == !=`）、逻辑（`&& || !`）、赋值（`= += -= *= /= %=`）。
- **控制流**：`if` / `else if` / `else`；`while`；`for`（支持 `for(int i=0; i<10; i++)` 语法糖）。
- **函数定义**：支持有参函数和返回值（`int add(int a, int b)`）。
- **递归**：**完全支持**。循环结构（`while`/`for`）将通过递归函数调用来实现。
- **注释**：支持 `// 单行注释` 和 `/* 多行注释 */`。

### 1.2 明确禁用的特性
- **指针类型**（`int *p`）、取地址符（`&`）和解引用（`*`）。
- **结构体/联合体**（`struct` / `union`）。
- **`switch` / `case` / `goto`**。
- **`continue` / `break`**。
- **标准库函数**（无法调用 `printf`，但需提供“内置函数”映射，见下文）。

---

## 2. 核心技术栈（C++ 构建）

AI 只需负责生成以下配置文件及项目骨架，**不生成核心编译逻辑代码**。

| 组件 | 选型 | 说明 |
| :--- | :--- | :--- |
| **构建系统** | **CMake** (最低版本 3.15) | 跨平台构建，需配置 CppParser 的查找路径。 |
| **C++ 标准** | **C++17** | 使用 `std::filesystem` 进行文件操作。 |
| **AST 生成器** | **CppParser** (GitHub: `cpp-parser/cpp-parser`) | 轻量无依赖的 C/C++ 解析器，生成 AST。你只需处理其节点。 |
| **JSON 生成** | **nlohmann/json** (单头文件) | 用于生成 `pack.mcmeta` 等配置文件。 |

### 2.1 CMake 集成 CppParser 的配置要点（AI 需生成）

AI 生成的 `CMakeLists.txt` 需包含以下逻辑：

```cmake
# 查找 CppParser 库（假设源码已放在项目子目录或系统路径中）
find_package(CppParser REQUIRED)

# 或者通过 add_subdirectory 直接引入（若将 CppParser 源码放入项目）
# add_subdirectory(third_party/cpp-parser)

add_executable(craftlang main.cpp ...)
target_link_libraries(craftlang PRIVATE CppParser::cppparser)
```

**依赖说明**：
- CppParser 唯一的外部依赖是 **Flex**（词法分析器），需确保系统已安装。
- CppParser 解析结果直接返回 AST，可通过其提供的 API 遍历节点。

---

## 3. 核心编译约束（供你设计算法时参考）

### 3.1 变量映射规则
- 每个 C 变量对应计分板上的一个 **目标（Objective）** 和一个 **虚拟玩家（Fake Player）**。
- 例如：`int a = 5;` 映射为 `scoreboard players set $c_a var_a 5`。

### 3.2 循环实现策略（利用递归）
由于原版数据包没有 GOTO，循环必须通过**递归调用**实现。例如 `while(cond) { body }` 的伪代码模板：
```mcfunction
# while_loop 函数
execute unless score ... run return 0   # 条件为假则退出
# 执行循环体...
function namespace:while_loop            # 递归调用自身
```
*需注意设置 `maxCommandChainLength` 足够大。*

### 3.3 函数返回值规则
- 使用一个全局记分板目标 `$return_value` 作为返回值槽位。
- 调用方执行 `function namespace:foo` 后，立即从 `$return_value` 读取结果。

### 3.4 内置函数库（Built-in API）
| C 语法调用 | 生成的 Minecraft 命令 |
| :--- | :--- |
| `print_int(a);` | `tellraw @a ...`（输出计分板值） |
| `print_str("Hello");` | `tellraw @a {"text":"Hello"}` |

---

## 4. 项目输出结构（数据包格式）

编译器执行后，生成以下目录结构到 `./output_datapack/`：

```
output_datapack/
├── pack.mcmeta                 # 数据包元信息（{"pack":{"pack_format":48,"description":"CraftLang编译生成"}}）
└── data/
    └── craftlang/              # 命名空间（固定为 craftlang）
        ├── functions/
        │   ├── main.mcfunction # 入口文件（对应源文件的 main 函数）
        │   ├── _if_1.mcfunction
        │   ├── _while_2.mcfunction
        │   └── ...
        └── advancements/
            └── root.json       # （可选）用于数据包加载时自动执行 main 函数
```

---

## 5. 输入示例（仅作为测试参考）

**输入源码 (`test.cl`)**：
```c
int main() {
    int a = 10;
    int b = 20;
    int c = a + b;
    print_int(c);
    return 0;
}
```

（编译器的实际输出由你的核心算法决定，此处不设预期模板。）

---

## 6. AI Agent 的具体任务

请 AI Agent 基于以上文档，**只生成以下内容**：

1. **`CMakeLists.txt`**：完整配置，包含 CppParser 和 nlohmann/json 的集成。
2. **项目目录骨架**：`src/`、`include/`、`third_party/`（如需）等。
3. **`main.cpp` 的初始框架**：仅包含读取源文件、调用 CppParser 解析、打印 AST 节点数量的测试代码（**不包含**核心编译逻辑）。
4. **`README.md`**：说明如何构建项目、安装依赖（Flex、CppParser）。

**禁止事项**：
- **不要**生成 `if/while` 到 `.mcfunction` 的具体转换代码。
- **不要**生成 AST 遍历并输出命令的实现。
- **不要**生成任何核心算法模块。

---

## 7. 给 AI Agent 的特殊指示

- 将 CppParser 作为**外部依赖**处理，提供两种集成方式供选择：`find_package` 或 `add_subdirectory`。
- 确保生成的 `main.cpp` 能够成功解析输入的 `test.cl` 文件，并调用 CppParser 的 API 打印 AST 结构（仅用于验证解析器工作正常）。
- 所有核心设计（AST 遍历、命令生成、符号表管理）留空，由开发者自行填充。

---

此文档已按你的要求调整。你可以直接将其喂给 AI，并指示：“**根据此文档生成 CMake 配置和项目骨架，集成 CppParser，不生成核心编译代码。**” 如果还有其他需要修改的地方，随时告诉我。