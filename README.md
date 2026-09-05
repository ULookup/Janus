# Janus Engine

Janus 是一个面向 Human Developer 与 AI Agent 的 C++20 2D 游戏引擎。项目希望让 Editor、Game 和 Agent 通过同一套 Engine Capability 理解、修改、运行并验证游戏世界。

项目已完成 **v0.1 Engine Foundation**、**v0.2 Renderer2D**、**v0.3 ECS + Scene**、**v0.4 Asset + Serialization**、**v0.5 Lua Gameplay Runtime** 和 **v0.6 Editor Foundation**。当前 Janus 已具备磁盘项目加载、稳定 UUID / AssetHandle、Lua Gameplay、离屏 Scene/Game View、Hierarchy、Inspector、CPU Picking、Asset Browser、Scene Save、Console、Scene Grid 与 Edit/Play 世界隔离。下一里程碑为 **v0.7 Reflection + Command**。

## 环境要求

- Windows 10/11
- Visual Studio 2022，安装“使用 C++ 的桌面开发”工作负载
- CMake 3.24 或更高版本
- Ninja
- Python 3，用于 glad OpenGL loader 生成；首次配置前安装 `jinja2`（`python -m pip install jinja2`）
- Git 与可访问 GitHub 的网络环境（依赖通过 CMake `FetchContent` 获取）

建议从 Visual Studio Developer PowerShell 或 Developer Command Prompt 运行以下命令，以确保 MSVC 工具链环境完整。

## 配置、构建与测试

开发构建：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
```

包含自动测试的构建：

```powershell
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
```

生成内容位于 `out/`。不要提交 `out/`、`.vs/`、二进制或本地 IDE 设置。

## v0.6 Editor 工作流

构建后可以启动 `JanusEditor` 打开 `SandboxProject`。Editor 的核心 authoring loop 为：

```text
ProjectSession
    ↓
EditorScene
    ├── Hierarchy
    ├── Inspector
    ├── Asset Browser
    ├── Console
    └── Scene View + EditorCamera + Grid
            ↓
        Edit + Save
            ↓
           Play
            ↓
Clone EditorScene -> RuntimeScene
            ↓
     ScriptEngine / Lua
            ↓
         Game View
            ↓
           Stop
            ↓
Discard RuntimeScene
```

Scene View 始终显示 `EditorScene`，并使用独立 EditorCamera；支持平移、缩放、自适应世界网格和 CPU Sprite Picking。Game View 在 Edit 状态预览 EditorScene 的主 Camera，进入 Play 后切换到隔离的 `RuntimeScene`。Play 期间作者态修改被 `EditorActions` 拒绝，Stop 后运行时改动不会回写 EditorScene。

Hierarchy 与 Scene View 共用 UUID-backed selection。Inspector 在 v0.6 手工支持 Transform、SpriteRenderer、Camera 和 LuaScript 等内置组件；Asset Browser 从 AssetRegistry 的确定性元数据枚举中选择 Texture / LuaScript 并进行类型安全赋值。所有成功的作者态修改都会标记 Scene dirty，Save 使用现有 atomic SceneSerializer 路径持久化。

Console 保存最近的 Editor 信息与可恢复错误，包括 Play/Stop、Save 和运行时/脚本失败入口；它是 v0.6 的最小错误可见性基础，不尝试提前实现 v0.9 的完整日志/Profiler/Audit 系统。

v0.6 的 Inspector mutation 被集中在 `EditorActions` seam；这是临时的 authoring capability 边界。v0.7 会把其内部实现替换为 Reflection + CommandBus，从而加入通用属性元数据和 Undo/Redo，而不要求重写 Editor panels。

## v0.5 磁盘项目与 Lua Gameplay 工作流

仓库提供最小可运行项目：

```text
SandboxProject/
├── Assets/
│   └── player.png
├── Scripts/
│   └── PlayerController.lua
├── Scenes/
│   └── Battle.scene
└── Config/
    └── AssetRegistry.json
```

运行链路：

```text
ProjectRuntimeConfig
    ↓
AssetRegistry.json + Battle.scene
    ↓
Persistent UUID / AssetHandle
    ↓
LuaScriptComponent
    ↓
ScriptEngine
    ↓
Input → Lua OnUpdate → Transform
    ↓
SceneRenderer
    ↓
Renderer2D
```

构建 `JanusSandbox` 时，CMake 会把 `SandboxProject` 复制到可执行文件旁边。直接启动 Sandbox 会默认加载该目录；也可以把自定义 project root 作为第一个命令行参数传入。

运行后可使用 **WASD 或方向键**移动 Player，Escape 退出。角色移动完全由 `Scripts/PlayerController.lua` 实现，不需要重新编译 JanusEngine。

ScriptEngine 每帧检查已使用脚本的文件修改时间。文件变化时会执行 `OnDestroy → 重新加载源码 → OnCreate`；v0.5 不保留 Lua table 的任意瞬时状态。若要直接编辑仓库中的脚本并观察热重载，建议启动 Sandbox 时显式传入仓库的 `SandboxProject` 作为 project root；默认启动使用的是构建后复制到可执行文件旁边的项目副本。

Scene 文件只保存 authoring/persistent state，不保存 ECS index/generation、GPU handle、Lua VM 状态或绝对资源路径。重复引用同一 `AssetHandle` 的资源通过 `AssetService` 命中同一 runtime cache。

## 目录

```text
Janus/
├── Engine/          引擎静态库与公共 API
├── Editor/          JanusEditor、EditorCore 与 authoring panels
├── Sandbox/         最小运行时客户端和验证程序
├── SandboxProject/  v0.6 Editor / Lua / Asset workflow fixture
├── Tests/           自动测试
├── docs/            PRD、技术架构和版本路线图
└── AGENTS.md        代码 Agent 的仓库级工作规则
```

## 设计文档

- [产品需求文档](docs/Janus%20Engine%20产品需求文档（PRD）.md)
- [技术架构设计](docs/Janus%20Engine%20技术架构设计.md)
- [版本路线图](docs/Janus%20Engine%20版本路线图.md)

## 当前原则

- 纵向闭环优先于横向功能数量。
- Engine Capability 先于 Editor 或 MCP 适配层。
- Core 不依赖 Renderer、Scene、Asset、Editor、MCP 或 Game。
- Persistent identity 与 runtime identity 分离。
- Scene/Component 保存 authoring state，不保存 GPU state。
- 可恢复错误使用显式 `Result`/`Error` 模型。
- 行为变更必须配套自动测试和可复现验证。

项目尚未选择开源 License。在明确 License 前，请不要假设代码可被重新分发或用于其他项目。
