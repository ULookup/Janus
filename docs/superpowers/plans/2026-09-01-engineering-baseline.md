# Janus Engineering Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立 Janus 第一套可提交、可复现、可测试并由 GitHub Actions 验证的 Windows/MSVC 工程基线。

**Architecture:** 仓库使用根目录规范文件定义协作契约，以 CMake Presets 作为本地与 CI 的唯一构建入口，以 Catch2 + CTest 提供纯 Core 自动测试。现有 Engine/Sandbox 边界保持不变，只修复 `SDLWindow.cpp` 错误传播给消费目标的问题。

**Tech Stack:** C++20、CMake 3.24+、Ninja、MSVC、SDL 3.4.14、spdlog 1.15.3、Catch2 3.15.3、GitHub Actions。

**Spec:** `docs/superpowers/specs/2026-09-01-engineering-baseline-design.md`

## Global Constraints

- 第一阶段只支持 Windows/MSVC + Ninja，CI 使用 `windows-latest`。
- 保持 CMake 3.24、C++20、SDL 3.4.14 和 spdlog 1.15.3 不变。
- 不引入 vcpkg、Conan、CPM、License、新引擎模块或全仓库格式化。
- 测试必须无窗口、无显示器、无 GPU 依赖。
- 不提交 `.vs/`、`out/` 或其他生成内容。
- 只暂存任务明确列出的文件，保留用户其他工作区内容。

---

### Task 1: 建立仓库入口与版本控制基线

**Files:**
- Create: `.gitignore`
- Create: `.gitattributes`
- Create: `.editorconfig`
- Create: `.clang-format`
- Create: `README.md`
- Create: `AGENTS.md`
- Track: `CMakeLists.txt`
- Track: `Engine/**`
- Track: `Sandbox/**`
- Track: `docs/Janus Engine 版本路线图.md`
- Track: `docs/Janus Engine 产品需求文档（PRD）.md`
- Track: `docs/Janus Engine 技术架构设计.md`

**Interfaces:**
- Consumes: 已确认的工程基线设计。
- Produces: 人类和 Agent 的统一仓库入口、稳定文本格式和可用 Git 基线。

- [ ] **Step 1: 记录缺失基线的失败检查**

Run:

```powershell
$required = '.gitignore','.gitattributes','.editorconfig','.clang-format','README.md','AGENTS.md'
$missing = $required | Where-Object { -not (Test-Path $_) }
if ($missing.Count -ne 0) { throw "Missing: $($missing -join ', ')" }
```

Expected: FAIL，列出六个缺失文件。

- [ ] **Step 2: 创建仓库规范文件**

`.gitignore` 至少包含：

```gitignore
# CMake and build output
/out/
/build/
CMakeUserPresets.json
compile_commands.json

# Visual Studio
/.vs/
*.sln
*.vcxproj*
*.user

# Compilers and debuggers
*.obj
*.o
*.lib
*.dll
*.exe
*.pdb
*.ilk

# Runtime output
*.log
logs/

# OS and editors
.DS_Store
Thumbs.db
.vscode/
.idea/
```

`.gitattributes`：

```gitattributes
* text=auto
*.h text eol=lf
*.cpp text eol=lf
*.cmake text eol=lf
CMakeLists.txt text eol=lf
*.json text eol=lf
*.md text eol=lf
*.yml text eol=lf
*.yaml text eol=lf
*.ps1 text eol=crlf
*.bat text eol=crlf
```

`.editorconfig`：

```ini
root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true
indent_style = space
indent_size = 4

[*.md]
trim_trailing_whitespace = false

[{CMakeLists.txt,*.cmake,*.json,*.yml,*.yaml}]
indent_size = 2
```

`.clang-format`：

```yaml
BasedOnStyle: LLVM
Language: Cpp
Standard: c++20
IndentWidth: 4
TabWidth: 4
UseTab: Never
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: Empty
NamespaceIndentation: None
SortIncludes: CaseSensitive
PointerAlignment: Left
DerivePointerAlignment: false
```

- [ ] **Step 3: 创建开发者和 Agent 入口**

`README.md` 必须包含：项目定位、当前 v0.1 状态、Windows 前置依赖、三个 Preset 命令、目录说明、文档链接和“不提交 out/.vs”的说明。

`AGENTS.md` 必须包含以下可执行约束：

````markdown
# Janus Agent Guide

## Mission
Janus is an Agent-native C++20 2D game engine. The current milestone is v0.1 Engine Foundation.

## Required workflow
1. Read the relevant files under `docs/` before changing architecture.
2. Search with `rg`/`rg --files` and preserve unrelated user changes.
3. Add or update automated tests for behavior changes.
4. Configure, build, and test with the repository presets before completion.
5. Report commands run and any remaining failures.

## Architecture boundaries
- Game, Editor, and MCP depend on the Engine public API.
- Core must not depend on Renderer, Scene, Asset, Editor, MCP, or Game.
- Platform backends remain behind platform interfaces.
- `.cpp` implementation files are private CMake sources.
- Use explicit `Result`/`Error` handling for recoverable failures.
- Do not introduce global Manager dependencies.

## Commands
```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
```

## Scope control
- Prefer a working vertical slice over parallel unfinished subsystems.
- Do not add Renderer, ECS, Scene, MCP, or Editor work unless the task requests it.
- Do not commit `.vs/`, `out/`, generated projects, binaries, or local settings.
````

- [ ] **Step 4: 验证规范文件和忽略规则**

Run:

```powershell
$required = '.gitignore','.gitattributes','.editorconfig','.clang-format','README.md','AGENTS.md'
$missing = $required | Where-Object { -not (Test-Path $_) }
if ($missing.Count -ne 0) { throw "Missing: $($missing -join ', ')" }
git check-ignore .vs out
git diff --check
```

Expected: 六个文件都存在；`.vs` 和 `out` 均被忽略；`git diff --check` 无输出。

- [ ] **Step 5: 建立源码基线提交**

Run:

```powershell
git add -- .gitignore .gitattributes .editorconfig .clang-format README.md AGENTS.md CMakeLists.txt Engine Sandbox 'docs/Janus Engine 版本路线图.md' 'docs/Janus Engine 产品需求文档（PRD）.md' 'docs/Janus Engine 技术架构设计.md'
git commit -m "chore: establish repository baseline"
```

Expected: 当前项目源码、原始设计文档和协作规范被跟踪；`.vs/`、`out/` 仍未进入提交。

---

### Task 2: 修复目标边界并提供可复现 Preset

**Files:**
- Modify: `Engine/CMakeLists.txt:13-33`
- Replace: `CMakePresets.json`

**Interfaces:**
- Consumes: 顶层 `JANUS_BUILD_SANDBOX` 和 `JANUS_BUILD_TESTS` 选项。
- Produces: `windows-msvc-debug` 与 `windows-msvc-debug-tests` configure/build presets，以及同名测试 preset。

- [ ] **Step 1: 重现完整构建失败**

Run from a Visual Studio Developer shell:

```powershell
cmake --build out/build/x64-Debug --config Debug
```

Expected: FAIL；Sandbox 编译传播过来的 `SDLWindow.cpp` 时找不到 `SDL3/SDL_error.h`。

- [ ] **Step 2: 修复 JanusEngine source 可见性**

将 `Engine/CMakeLists.txt` 的 source 列表整理为：

```cmake
target_sources(
    JanusEngine
    PRIVATE
        Core/Log/Log.cpp
        Platform/SDL/SDLPlatform.cpp
        Platform/SDL/SDLWindow.cpp
    PUBLIC
        Core/Base.h
        Core/Types.h
        Core/Assert.h
        Core/Error/Error.h
        Core/Error/Result.h
        Core/Log/Log.h
        Platform/Platform.h
        Platform/Window/WindowConfig.h
        Platform/Window/Window.h
        Platform/SDL/SDLWindow.h
)
```

- [ ] **Step 3: 添加 CMake Presets**

使用 schema version 5，`CMakePresets.json` 定义：

```json
{
  "version": 5,
  "cmakeMinimumRequired": { "major": 3, "minor": 24, "patch": 0 },
  "configurePresets": [
    {
      "name": "windows-msvc-debug",
      "displayName": "Windows MSVC Debug",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "JANUS_BUILD_SANDBOX": "ON",
        "JANUS_BUILD_TESTS": "OFF"
      }
    },
    {
      "name": "windows-msvc-debug-tests",
      "displayName": "Windows MSVC Debug with Tests",
      "inherits": "windows-msvc-debug",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": { "JANUS_BUILD_TESTS": "ON" }
    }
  ],
  "buildPresets": [
    { "name": "windows-msvc-debug", "configurePreset": "windows-msvc-debug" },
    { "name": "windows-msvc-debug-tests", "configurePreset": "windows-msvc-debug-tests" }
  ],
  "testPresets": [
    {
      "name": "windows-msvc-debug-tests",
      "configurePreset": "windows-msvc-debug-tests",
      "output": { "outputOnFailure": true }
    }
  ]
}
```

- [ ] **Step 4: 验证无测试构建闭环**

Run:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

Expected: configure 和 build 均 exit 0；`JanusEngine` 与 `JanusSandbox` 成功编译。

- [ ] **Step 5: 提交构建修复**

```powershell
git add -- CMakePresets.json Engine/CMakeLists.txt
git commit -m "build: add reproducible MSVC presets"
```

---

### Task 3: 接入 Catch2 与 Core 自动测试

**Files:**
- Modify: `CMakeLists.txt:129-142`
- Create: `Tests/CMakeLists.txt`
- Create: `Tests/Core/ResultTests.cpp`

**Interfaces:**
- Consumes: `JanusEngine`、`Result<T>`、`Result<void>`、`JANUS_BUILD_TESTS`。
- Produces: `JanusTests` 和由 `catch_discover_tests` 注册的 CTest cases。

- [ ] **Step 1: 验证测试 Preset 当前失败**

Run:

```powershell
cmake --preset windows-msvc-debug-tests
```

Expected: FAIL，因为 `JANUS_BUILD_TESTS=ON` 时 `Tests/` 尚不存在。

- [ ] **Step 2: 添加 Catch2 固定依赖**

在顶层 `if(JANUS_BUILD_TESTS)` 中加入：

```cmake
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

enable_testing()
add_subdirectory(Tests)
```

- [ ] **Step 3: 创建测试目标**

`Tests/CMakeLists.txt`：

```cmake
add_executable(
    JanusTests
    Core/ResultTests.cpp
)

target_link_libraries(
    JanusTests
    PRIVATE
        JanusEngine
        Catch2::Catch2WithMain
)

janus_set_warnings(JanusTests)

include(Catch)
catch_discover_tests(JanusTests)
```

- [ ] **Step 4: 添加 Result characterization tests**

`Tests/Core/ResultTests.cpp` 覆盖：

```cpp
#include "Core/Error/Result.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Result value success exposes its value", "[core][result]")
{
    auto result = Janus::Result<int>::Success(42);
    REQUIRE(result.HasValue());
    REQUIRE_FALSE(result.HasError());
    REQUIRE(static_cast<bool>(result));
    REQUIRE(result.Value() == 42);
}

TEST_CASE("Result value failure exposes its error", "[core][result]")
{
    auto result = Janus::Result<int>::Failure(
        Janus::ErrorCode::InvalidArgument,
        "invalid value");
    REQUIRE_FALSE(result.HasValue());
    REQUIRE(result.HasError());
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.GetError().code == Janus::ErrorCode::InvalidArgument);
    REQUIRE(result.GetError().message == "invalid value");
}

TEST_CASE("Void result represents success", "[core][result]")
{
    auto result = Janus::Result<void>::Success();
    REQUIRE(result.HasValue());
    REQUIRE_FALSE(result.HasError());
    REQUIRE(static_cast<bool>(result));
}

TEST_CASE("Void result represents failure", "[core][result]")
{
    auto result = Janus::Result<void>::Failure(
        Janus::ErrorCode::PlatformInitFailed,
        "platform failed");
    REQUIRE_FALSE(result.HasValue());
    REQUIRE(result.HasError());
    REQUIRE_FALSE(static_cast<bool>(result));
    REQUIRE(result.GetError().code == Janus::ErrorCode::PlatformInitFailed);
    REQUIRE(result.GetError().message == "platform failed");
}
```

- [ ] **Step 5: 验证测试构建与执行**

Run:

```powershell
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
```

Expected: 三条命令 exit 0；CTest 发现 4 个 case，0 failures。

- [ ] **Step 6: 提交测试基线**

```powershell
git add -- CMakeLists.txt Tests
git commit -m "test: add Catch2 core test baseline"
```

---

### Task 4: 添加 GitHub Actions 并执行最终验收

**Files:**
- Create: `.github/workflows/ci.yml`
- Verify: `README.md`
- Verify: `AGENTS.md`

**Interfaces:**
- Consumes: `windows-msvc-debug-tests` configure/build/test presets。
- Produces: push/PR 自动构建和测试状态。

- [ ] **Step 1: 验证 CI 文件缺失**

Run:

```powershell
if (-not (Test-Path '.github/workflows/ci.yml')) { throw 'CI workflow is missing' }
```

Expected: FAIL，提示 CI workflow 缺失。

- [ ] **Step 2: 创建 Windows CI**

`.github/workflows/ci.yml`：

```yaml
name: CI

on:
  push:
  pull_request:

permissions:
  contents: read

jobs:
  windows-msvc:
    runs-on: windows-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v6

      - name: Configure MSVC environment
        uses: ilammy/msvc-dev-cmd@v1

      - name: Configure
        run: cmake --preset windows-msvc-debug-tests

      - name: Build
        run: cmake --build --preset windows-msvc-debug-tests

      - name: Test
        run: ctest --preset windows-msvc-debug-tests
```

- [ ] **Step 3: 验证命令引用一致**

Run:

```powershell
$preset = 'windows-msvc-debug-tests'
foreach ($file in 'README.md','AGENTS.md','.github/workflows/ci.yml','CMakePresets.json') {
    if (-not (Select-String -Quiet -LiteralPath $file -Pattern $preset)) {
        throw "$file does not reference $preset"
    }
}
git diff --check
```

Expected: exit 0，无 whitespace errors。

- [ ] **Step 4: 执行完整本地验收**

Run from a Visual Studio Developer shell:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
git status --short --ignored
```

Expected:

- 两套 configure/build 均 exit 0；
- CTest 4/4 passed；
- `.vs/` 和 `out/` 显示为 ignored；
- 没有未预期的生成文件进入待提交区。

- [ ] **Step 5: 提交 CI 并检查最终历史**

```powershell
git add -- .github/workflows/ci.yml
git commit -m "ci: build and test Janus on Windows"
git log --oneline --decorate -n 6
git status --short
```

Expected: CI commit 存在；除用户明确保留的文件外工作区干净。
