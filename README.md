# Janus Engine

Janus 是一个面向 Human Developer 与 AI Agent 的 C++20 2D 游戏引擎。项目希望让 Editor、Game 和 Agent 通过同一套 Engine Capability 理解、修改、运行并验证游戏世界。

项目已完成 **v0.1 Engine Foundation**、**v0.2 Renderer2D**、**v0.3 ECS + Scene** 和 **v0.4 Asset + Serialization**。当前引擎已经能够从磁盘项目数据加载 AssetRegistry 与 Scene，通过稳定 UUID / AssetHandle 恢复 World，按需解析并缓存纹理资源，再交由 SceneRenderer / Renderer2D 渲染。下一里程碑为 **v0.5 Lua Gameplay Runtime**。

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

## v0.4 磁盘项目工作流

仓库提供最小可运行项目：

```text
SandboxProject/
├── Assets/
│   └── player.png
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
AssetService load + cache
    ↓
SceneRenderer
    ↓
Renderer2D
```

构建 `JanusSandbox` 时，CMake 会把 `SandboxProject` 复制到可执行文件旁边。直接启动 Sandbox 会默认加载该目录；也可以把自定义 project root 作为第一个命令行参数传入。

Scene 文件只保存 authoring/persistent state，不保存 ECS index/generation、GPU handle 或绝对资源路径。重复引用同一 `AssetHandle` 的 Sprite 会通过 `AssetService` 命中同一 runtime texture cache。

## 目录

```text
Janus/
├── Engine/          引擎静态库与公共 API
├── Sandbox/         最小引擎客户端和运行验证程序
├── SandboxProject/  v0.4 磁盘项目与资源 Fixture
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
