---
name: CraftLang 提交与推送
description: 在 CraftLang 仓库执行提交前检查、生成提交信息并自动推送时使用。触发词：提交、commit、push、推送、准备提交、检查后提交、帮我提交。流程包含工作区/diff 卫生检查、全量构建与 warning 检查、按模块行为验证、提交前自动 clang-tidy lint、提交信息规范，以及 commit 后自动 push 当前分支。
---

# CraftLang 提交与推送

用于在 CraftLang 仓库完成一次安全的提交：先检查、构建、lint 和行为验证，最后 commit 并 push。

## 硬性规则

- **只有用户明确要求提交时才执行。** 用户问“能不能提交”“准备提交了吗”，只回答检查结果，不执行 `git add`、`git commit`、`git push`。
- **默认 commit 后自动 push 当前分支到 `origin`。** 如果用户说“只提交”“先别推送”“不要 push”，才跳过 push。
- **每次提交前都必须自动运行 lint。** 在 `tools/lint.ps1` 通过之前，不得执行 `git add`、`git commit`、`git push`。
- 只有用户明确说“这次跳过 lint”时，才允许跳过，并在最终报告里记录“用户要求跳过”。
- lint 失败时，报告文件、行号、规则名和诊断文本后停止；lint 问题由用户自己修，不要直接修改源码或 `.clang-tidy`，除非用户明确要求。
- 检查失败就停止，不允许“先提交再说”。
- 禁止 `git push --force`、`git push --force-with-lease`，除非用户明确要求且确认目标分支。
- 禁止 `git add .`、`git add -A` 盲扫；必须显式暂存文件。
- 不提交 `build/`、`tmp/`、`.vscode/`、临时测试程序、二进制、密钥、日志、数据库文件。
- 不执行交互式命令（不带 `-m` / `-F` 的 `git commit`、`rebase -i` 等），避免卡住。
- 不 amend、不 rebase 已经 push 的提交，除非用户明确要求。
- 如果 commit 成功但 push 失败，保留本地 commit，报告错误；不要 reset、不要删除提交。
- **文档同步属于提交内容。** 每次提交前对照 diff 检查 `README.md` 和 `design.md`：行为、CLI、配置格式、已知限制或模块设计有变化时必须同步更新，并随代码一起提交；确认无需更新时，在最终报告中写明“文档无需更新”及理由。

## 一、确认当前状态

在仓库根目录运行：

```powershell
git rev-parse --show-toplevel
git branch --show-current
git status --short
git status -sb
git diff --stat
git diff --name-status
git diff
git diff --cached
git diff --check
git diff --cached --check
```

检查：

- 当前目录必须是仓库根目录。
- 必须处于正常分支；detached HEAD 时停止。
- 不能处于 merge / rebase / cherry-pick 中间状态；有冲突时停止。
- 只能包含本次任务相关的改动。
- 如果已有暂存内容，先确认它是否属于本次提交；有无关暂存内容时停止并询问用户。
- 未跟踪文件要逐个判断：是本次要提交的新文件，还是应该忽略的临时产物。
- 用 grep 工具搜索冲突标记，排除 `third_party/`、`build*/`、`tmp/`：

```text
^(<<<<<<<|=======|>>>>>>>)( .*)?$
```

- 检查 diff 中是否混入调试残留：无条件 `std::cout`、`printf`、`TODO`、`FIXME`、临时日志、绝对路径。已有的 `#ifdef _DEBUG` 输出不算。
- 检查是否包含密钥、token、密码、私钥或 `.env`；发现即停止。
- 用户可能正在同时编辑文件。**在 `git add` 前必须重新检查一次 `git status` 和 diff**；如果文件在检查后发生变化，重新构建和验证。

## 二、CMake 配置检查

项目使用 `file(GLOB_RECURSE)` 收集 `src/` 下的源文件，因此：

- 如果 diff 中有 `src/**/*.cpp` 或 `src/**/*.c` 的新增/删除，必须重新运行 CMake 配置。
- 如果 `CMakeLists.txt` 有改动，也必须重新配置。

```powershell
# 仅在需要重新配置时运行
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
```

构建需要 libclang：

```powershell
$env:LIBCLANG_PATH
```

如果该环境变量为空或路径无效，停止并询问用户 libclang 的实际路径，不要猜测。

## 三、Lint 与编译检查

仓库根目录有 `.clang-tidy`，`tools/lint.ps1` 会读取 `build/compile_commands.json`，对 `src/` 下的翻译单元运行 clang-tidy。

提交前必须运行：

```powershell
pwsh -NoProfile -File tools/lint.ps1
```

规则：

- 默认使用 `--warnings-as-errors=*`，只要出现诊断就退出 1。
- 脚本不自动修复，也不要手工替用户改源码。
- 只查看诊断时可加 `-NoWarningsAsErrors`，但提交门禁必须用默认模式。
- lint 失败时原样报告文件、行号、规则名和诊断文本，由用户自行修复。
- 如果新增了 `src/**/*.cpp`，先重新运行 CMake 配置，确保 `compile_commands.json` 包含它。

全量构建：

```powershell
cmake --build build --target all -j 12
```

要求：

- 退出码必须为 0。
- 捕获并检查输出中的 `warning:`；本次改动引入的新 warning 视为失败。
- 如果存在项目原有 warning，先确认它不是本次引入的，并在最终报告里说明。
- 如果构建目录不存在或配置损坏，先按上一节重新配置。
- 已知构建配置缺口：`craftlinker` 目标目前没有加 `-Wall -Wextra`。改 linker 时，不能只凭“构建没有 warning”判断代码质量，必须运行 lint，并手动执行一次示例链接确认输出。

## 四、行为验证

所有命令都在仓库根目录执行。**任何 C++ 代码改动都要先完成全量构建。**

### 1. craftsim / 模拟器改动

```powershell
build\craftsim.exe
```

当前示例 `example/test/data/test/function/test.mcfunction` 的关键预期：

- 输出包含 `output: 123456`
- 输出包含 `"this is a test"`
- 输出包含 `return(7:0): 0`
- 不包含 `Unknown command: ,position: 0:0`
- `scoreboard` / `execute` 的 `Unknown command` warning 目前是预期的，因为这两个命令尚未实现；不要把这类预期 warning 当成失败。
- 如果本次改动修改了 `example/test/...`，预期输出要按新内容同步确认。

如果改动涉及分词器、`say`、`return` 或 `executeFunction`，额外验证：

- `say hello world` 输出整行消息；
- `say "this is a test"` 保留引号；
- 空 `say` / 只有空格的 `say` 报 `say command requires a message argument`；
- 最后一行没有换行时正常结束；
- CRLF 文件不会产生行号错位或额外空命令。

做额外用例时，可以临时修改 `example/test/data/test/function/test.mcfunction`，但提交前必须恢复：

```powershell
git restore -- example/test/data/test/function/test.mcfunction
git status --short
```

不要把临时测试数据带进提交。

### 2. 编译器改动

```powershell
build\craftlang.exe example\test.c build
build\craftlang.exe example\test2.c build
build\craftlang.exe example\test_scope.c build
```

检查：

- 退出码为 0；
- 生成 `build\test_symbols.json`、`build\test2_symbols.json`、`build\test_scope_symbols.json`；
- 生成对应的 `build\*_o\function\` 目录和 `.mcfunction` 文件。

不要用 `example\test_while.c`、`example\test_for.c` 作为成功用例；它们当前本来就因为不支持循环而失败。

### 3. Linker / 链接行为

修改 linker 后，手动运行一次示例链接确认输出：

```powershell
build\craftlinker.exe example\setting.json
```

检查 `example\setting.json` 中 `outputPath` + `name` 指向的数据包目录：

- `pack.mcmeta` 是合法 JSON；
- `data/minecraft/tags/function/load.json` 引用的函数真实存在；
- `.mcfunction` / `.json` / `.mcmeta` 中没有残留 `.f.N.f.`、`.g.N.g.`、`..name..` 占位符；
- 输出中至少有一个 `.mcfunction`。

这一步只是手工行为确认，不是提交门禁；静态检查由 `tools/lint.ps1` 负责。发现链接问题时报告给用户，不要直接改源码。

### 4. mcstd 改动

如果修改了 `mcstd/`，同样执行上面的示例链接，确认链接后的数据包内容。

### 5. 仅文档 / skill 改动

至少完成全量构建、lint 和 diff 检查；如果改动只涉及 Markdown 且不触碰构建，可在报告中说明未运行行为测试的原因。

### 6. 测试后的清理检查

行为测试可能生成文件或临时修改示例。提交前再次运行：

```powershell
git status --short
git diff --check
```

确保：

- 没有把 `build/`、`tmp/`、测试输出加入暂存区；
- 没有残留的临时测试文件；
- 没有把临时修改的示例文件带进提交。

## 五、提交前自动 Lint（强制）

在 `git add` / `git commit` 之前，必须运行：

```powershell
pwsh -NoProfile -File tools\lint.ps1
```

规则：

- 脚本退出码必须为 0；否则不得 commit，也不得 push。
- 失败时报告文件、行号、规则名和诊断文本；不要自行修改源码或 `.clang-tidy` 来让 lint 通过。
- lint 问题由用户自己修；只有用户明确要求时才代为修改源码。
- 只有用户明确说“这次跳过 lint”时，才允许跳过，并在最终报告中记录。
- 用户修复后，重新运行脚本；通过后再继续提交流程。

## 六、生成提交信息

### 格式

```text
type(scope): 中文摘要

- 改动点 1
- 改动点 2

验证：
- 实际运行过的命令
- 实际观察到的结果
```

### 类型

- `feat`：新功能
- `fix`：Bug 修复
- `refactor`：不改变行为的重构
- `docs`：文档
- `test`：测试
- `build` / `chore`：构建系统、工具、杂项
- `perf`：性能优化

### Scope

使用项目模块名：

- `compiler`
- `linker`
- `sim`
- `mcstd`
- `build`
- `docs`
- `example`
- 跨模块可写 `compiler/linker`、`sim/function`

### 规则

- 标题具体，不写“更新代码”“修复问题”“提交改动”。
- 标题不超过约 72 个字符。
- 正文说明“做了什么、为什么”，不要逐行复述 diff。
- **只写实际跑过的验证。** 没跑的行为不能写成已通过。
- 行为、CLI、已知限制变化时，同步更新 `README.md` / `design.md`（见硬性规则）。
- 破坏性改动加：

```text
BREAKING CHANGE: 说明
```

### 示例

```text
feat(sim): 支持行模式分词并完善 say/return 边界处理

- 新增 TOKENIZE_LINE：整行截取、跳过前导空格、不消费换行符
- CRLF 仍由 TOKENIZE_NORMAL 产生唯一的 NEWLINE token
- say 改为整行消息，保留引号，空消息报错
- executeFunction 先检查 END 再 tokenize，保证 handler 结束时停在 NEWLINE/END

验证：
- cmake --build build --target all -j 12
- build\craftsim.exe
```

多行提交信息建议写入 `tmp/commit-msg.txt`（`tmp/` 已被 gitignore），避免 PowerShell 引号/换行转义问题：

```powershell
git commit -F tmp/commit-msg.txt
```

提交后删除临时消息文件。

## 七、暂存与提交

lint 通过后，显式暂存文件：

```powershell
git add -- path/to/file1 path/to/file2
```

不要使用：

```powershell
git add .
git add -A
```

暂存后再次检查：

```powershell
git diff --cached --check
git diff --cached
git status --short
```

确认暂存内容只包含本次改动后提交：

```powershell
git commit -F tmp/commit-msg.txt
```

提交后检查：

```powershell
git status --short
git log -1 --stat
```

如果暂存后没有实际改动，报告“没有可提交的改动”，不要创建空提交。

## 八、自动 Push

默认在 commit 成功后立即 push 当前分支：

```powershell
git branch --show-current
git remote get-url origin
git fetch origin
git status -sb
```

判断：

- detached HEAD：停止。
- 本地分支落后于远端：停止并询问用户是否 rebase/merge；不要自动 pull、不要 force push。
- 没有 upstream：使用 `git push -u origin HEAD`。
- 正常情况：

```powershell
git push origin HEAD
```

规则：

- 永远不要 force push。
- fetch 因网络失败时，可以继续尝试 push；如果 push 也失败，保留本地 commit 并报告。
- push 被拒绝（non-fast-forward / 权限 / 认证）时停止，报告原因，不自动重试危险操作。
- push 成功后验证：

```powershell
git status -sb
git log -1 --oneline
```

## 九、失败处理

| 情况 | 处理 |
|---|---|
| 构建失败 | 修复或停止，不提交 |
| 新增 warning | 修复或明确说明，不提交 |
| 行为测试失败 | 停止，不提交 |
| lint 失败 | 报告文件、行号和诊断，停止，不 commit / push |
| lint 规则误报 | 说明情况并询问用户；不要擅自修改 `.clang-tidy` |
| 有无关暂存内容 | 停止并询问用户 |
| merge / rebase / detached HEAD | 停止，不提交 |
| push 被拒绝 | 保留本地 commit，报告错误，不 force push |
| 认证 / 网络失败 | 保留本地 commit，报告错误 |

## 十、最终报告

提交并推送后，向用户报告：

```text
提交完成：
- 分支：<branch>
- 提交：<hash> <title>
- Push：origin/<branch> 成功
- 构建：cmake --build build --target all -j 12 通过
- 行为：<实际运行的测试> 通过
- Lint：tools/lint.ps1 通过（N 个文件）/ 用户要求跳过
- 文档：README.md / design.md 已同步更新 / 无需更新（<理由>）
- Warning：无新增 / 说明已有 warning
```

只提交未推送时：

```text
- Push：未推送（用户要求只提交）
```

push 失败时：

```text
- 本地提交已保留：<hash>
- Push 失败：<错误摘要>
- 未执行 force push
```

## 项目速查

- 工作目录：仓库根目录
- 构建目录：`build/`（gitignored）
- 构建系统：CMake + MinGW Makefiles，Debug
- 编译器：llvm-mingw clang++
- `craftsim` 硬编码读取 `example/test`，必须在仓库根目录运行
- `build/`、`tmp/`、`.vscode/` 不提交
- lint 配置：仓库根目录 `.clang-tidy`
- lint 命令：`pwsh -NoProfile -File tools/lint.ps1`
- lint 只检查 `src/` 下的翻译单元，不自动修复；诊断由用户自行修复
- `craftlinker` 当前没有 `-Wall -Wextra`，改 linker 时除 lint 外还要手动跑一次示例链接
- Windows 下 `%TEMP%` 的未签名临时 exe 可能被杀软拦截；优先使用 `build/` 目标和 `tmp/` 目录，不要反复重试被杀文件
