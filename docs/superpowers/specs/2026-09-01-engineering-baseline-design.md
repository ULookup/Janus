# Janus 工程基线设计

## 1. 目标

为 Janus 建立第一套可提交、可复现、可验证、适合人类与 AI Agent 协作的工程基线。完成后，新开发者或 Agent 应能仅依赖仓库内说明完成配置、构建和测试，并且 GitHub Actions 能验证同一流程。

本阶段不扩展游戏引擎功能，不实现 Renderer、ECS、Scene 或 MCP，也不调整现有产品路线。

## 2. 方案选择

考虑过三种方案：

1. 仅补文档和忽略规则：变更最小，但无法解决当前构建失败和不可验证问题。
2. 完整轻量基线：补齐协作规范、构建预设、测试入口、CI，并修复现有构建边界。能够立即形成工程闭环，采用此方案。
3. 一次性引入包管理器、静态分析矩阵、覆盖率和多平台 CI：长期能力更强，但超出当前 857 行源码项目的需要。

## 3. 交付范围

### 3.1 仓库入口

- `README.md`：说明定位、当前阶段、依赖、配置、构建、测试和目录结构。
- `AGENTS.md`：定义 Agent 的工作边界、架构约束、构建命令、测试要求和完成标准。
- `.gitignore`：忽略 Visual Studio、CMake/Ninja、构建产物、日志和操作系统临时文件。
- `.gitattributes`：固定文本文件行尾策略，避免 Windows/Linux 协作产生无意义差异。
- `.editorconfig`：统一 UTF-8、换行、缩进和尾随空白规则。
- `.clang-format`：提供 C++ 自动格式化规则；本阶段不批量重排现有源码。

不新增 License。后续需要公开分发时再由项目所有者明确选择。

### 3.2 构建系统

- 将空的 `CMakePresets.json` 替换为可用的 Windows/MSVC + Ninja 预设。
- 提供 Debug 开发预设和启用测试的 Debug 预设。
- 保持最低 CMake 版本 3.24、C++20、SDL3 和 spdlog 版本不变。
- 修复 `Engine/CMakeLists.txt` 中 `SDLWindow.cpp` 被声明为 `PUBLIC` source 的问题；实现文件必须为 `PRIVATE`，公共头文件继续作为 `PUBLIC` sources。
- 开启 CTest，并让测试目标通过同一 warning policy 编译。

不引入 vcpkg、Conan 或 CPM；现阶段继续使用已有 `FetchContent`。

### 3.3 自动测试

- 使用 Catch2 v3，通过 `FetchContent` 获取并固定版本。
- 新建 `JanusTests` 可执行目标并接入 CTest。
- 第一批测试覆盖纯 Core 能力，优先验证 `Result<T>`、`Result<void>` 的成功/失败状态和值/错误访问。
- 测试不得依赖窗口、显示器或图形环境，保证本地与 CI 都能稳定运行。
- 本阶段不为 SDL Window 编写 GUI 集成测试。

### 3.4 持续集成

- 新增 GitHub Actions 工作流，在 `push` 和 `pull_request` 时运行。
- 第一阶段只使用 `windows-latest`，与当前唯一已验证的 MSVC 开发环境一致。
- CI 按仓库预设执行 configure、build 和 CTest；不复制另一套构建参数。
- CI 不发布产物、不创建 Release、不上传覆盖率。

### 3.5 Agent 协作约束

根目录 `AGENTS.md` 对整个仓库生效，至少包含：

- 产品目标与当前里程碑；
- 模块依赖方向和第三方库边界；
- 推荐的文件搜索、配置、构建和测试命令；
- 修改前后必须执行的验证；
- 禁止提交 `out/`、`.vs/` 等生成内容；
- 新增功能需遵循纵向闭环、显式错误和测试优先原则；
- Agent 不得擅自推进 Renderer、ECS、MCP 等未在当前任务范围内的模块。

## 4. 文件职责

| 文件 | 职责 |
| --- | --- |
| `README.md` | 面向开发者的项目入口和快速开始 |
| `AGENTS.md` | 面向代码 Agent 的仓库级执行规则 |
| `.gitignore` | 排除生成文件和本地状态 |
| `.gitattributes` | 跨平台 Git 文本规范 |
| `.editorconfig` | 编辑器基础格式规范 |
| `.clang-format` | C++ 格式化规范 |
| `CMakePresets.json` | 本地和 CI 的统一配置入口 |
| `CMakeLists.txt` | 测试依赖与顶层测试开关 |
| `Engine/CMakeLists.txt` | 引擎目标的源码可见性修复 |
| `Tests/CMakeLists.txt` | 测试目标和 CTest 注册 |
| `Tests/Core/ResultTests.cpp` | `Result` 行为测试 |
| `.github/workflows/ci.yml` | Windows 构建和测试流水线 |

## 5. 验证流程

在 Visual Studio Developer PowerShell 或 Developer Command Prompt 中执行：

```powershell
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
```

验收标准：

1. 从无构建目录状态可以成功配置；
2. `JanusEngine`、`JanusSandbox`、`JanusTests` 均成功编译；
3. CTest 报告零失败；
4. Git 状态不包含 `.vs/`、`out/` 或其他生成文件；
5. README 与 AGENTS 中的命令和实际 Preset 名称一致；
6. CI 使用完全相同的 Preset 名称。

## 6. 非目标

本阶段明确不包含：

- 新游戏引擎模块或运行时功能；
- Linux/macOS CI；
- Sanitizer、覆盖率、性能 Benchmark；
- 包管理器迁移；
- 文档站点、发布流水线或安装包；
- 全仓库格式化；
- 开源 License 选择。

## 7. 风险控制

- `FetchContent` 依赖网络：通过固定 Git tag 保持版本确定性，后续再评估依赖锁定或包管理器。
- GUI 程序不适合无头 CI：CI 只编译 Sandbox，不启动窗口；自动测试保持纯 Core。
- CMake Preset 在非 Developer Shell 中可能找不到 MSVC：README 明确要求使用 VS 开发者终端，GitHub Actions 使用已安装的 VS 工具链。
- 当前仓库没有历史提交：工程化实施时只暂存本任务文件，避免把 `.vs/` 和 `out/` 纳入版本控制。
