# Janus Engine

Janus 是一个面向 Human Developer 与 AI Agent 的 C++20 2D 游戏引擎。项目希望让 Editor、Game 和 Agent 通过同一套 Engine Capability 理解、修改、运行并验证游戏世界。

项目已完成 **v0.1 Engine Foundation**、**v0.2 Renderer2D**、**v0.3 ECS + Scene**、**v0.4 Asset + Serialization**、**v0.5 Lua Gameplay Runtime**、**v0.6 Editor Foundation**、**v0.7 Reflection + Command** 和 **v0.8 MCP Agent Foundation**。当前 Janus 已具备磁盘项目加载、稳定 UUID / AssetHandle、Lua Gameplay、离屏 Scene/Game View、metadata-driven Inspector、Reflection-backed Scene persistence、CommandBus、Undo/Redo，以及原生 C++ MCP stdio Agent authoring 能力。**v0.9 Agent Development Loop** 已通过 PR #81 合入 main，包含 Runtime 调试、共享诊断、作者态事务及 Activity；本地验收与集成状态见[实施记录](docs/verification/2026-09-06-v0.9-agent-development-loop.md)。当前正在开发 **v0.10 Game Systems**，首个 Project Settings + Action Input 切片已通过 PR #82 合入 main（`c8a7c06`）。整体顺序见[开发路线](docs/superpowers/plans/2026-09-06-next-development-roadmap.md)，下一阶段建议先共享 Runtime 执行阶段，再完成可玩 UI，详见[最新 main 调研与路线提案](docs/superpowers/plans/2026-09-07-next-stage-runtime-ui-roadmap.md)。

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

## v0.10 Stage A：Project Settings + Action Input

10-03 共享 Runtime 已通过 PR #83 合入 main（`c71ad50`），[验收记录](docs/verification/2026-09-07-v0.10-shared-runtime.md)。10-04a UI 布局的 PR #84 已合入其 shared-runtime 父分支，尚未进入该 main 基线，[验收记录](docs/verification/2026-09-07-v0.10-ui-layout.md)。10-04b Text/离线字体图集、Lua 文本更新、Inspector 多行编辑、Font 赋值和 MCP `assets.search` 已在 `codex/v0.10-ui-text` 本地实现；试用与验证见[Text 验收记录](docs/verification/2026-09-07-v0.10-ui-text.md)。10-05a Button/事件/输入消费和计数菜单已本地实现，下一包为 10-05b 战斗/结构化快照。

当前开发切片增加 `project.json` 和 Editor 底部的 **Project Settings** 标签，可配置默认 Scene、资源路径、游戏分辨率、VSync、Target FPS 和多个按键到 Action 的映射。没有 manifest 的旧项目仍使用原路径；非法配置明确报错。SandboxProject 已提供示例配置。

Lua 可以使用 `Input.is_action_down("MoveLeft")`、`Input.was_action_pressed(...)`、`Input.was_action_released(...)` 和 `Input.has_action(...)`；旧 key API 保留。Game View 接收其显示区域内的输入，工具面板和黑边不向 Gameplay 传递键鼠。`Input.pointer_position()` 返回左上原点的项目逻辑坐标或 nil，鼠标按钮支持 Left/Right/Middle。

设置保存独立于 Scene 保存及 Undo/事务，运行中、事务中和故障恢复期间会被拒绝。Input 下次 Play 生效，Scene/registry 路径下次打开项目生效，VSync/FPS 下次启动生效。完整配置、验证和后续边界见 [Stage A 实施记录](docs/verification/2026-09-07-v0.10-project-input.md)。当前尚未完成或发布整个 v0.10。

## v0.9 Agent Development Loop

Editor 底部新增 Profiler / Agent Activity 标签，Console 支持日志级别过滤。运行控制增加 Pause、Resume、Step；Lua 失败保留 Faulted 场景直到 Stop。暂停单步使用固定 1/60 秒和空输入。

MCP 新增 `runtime.play/pause/stop/step`、`transaction.begin/commit/rollback`，以及 runtime/status、runtime/entity/{uuid}、logs/recent、profiler/latest-frame、transaction/status、agent/activity 六类资源。资源和工具参数、事务限额及恢复流程见[完整使用与验收记录](docs/verification/2026-09-06-v0.9-agent-development-loop.md)。

Agent 事务的子命令携带 `transaction` UUID，在同一 EditorScene 暂时可见；Commit 后形成一条 Human Undo 历史。跨连接写入、运行中作者态写入和事务期间 Save/Play 被核心入口拒绝。失败、超时或 stdio 断连自动回滚；无法回滚时进入显式恢复状态。

CPU 与渲染统计、日志和命令回执共用 Engine 数据，保留有界历史。版本号从 CMake PROJECT_VERSION 统一传给 MCP。

## v0.8 MCP Agent Foundation 工作流

v0.8 让外部 Agent 成为 Janus 的正式 Engine Client。生产路径仍由 `JanusEditor` 承载：

```powershell
JanusEditor --project <project-root> --mcp-stdio
```

启用 MCP stdio 后，stdout 只用于协议帧，Janus 日志与诊断输出到 stderr。当前支持主协议 `2026-07-28`，同时兼容 `2025-11-25` 的 legacy initialize 生命周期。

整体 authoring 路径为：

```text
External MCP Client
        │
        ▼
JSON-RPC / stdio
        │
        ▼
   McpEditorHost
        │
 protocol worker
        │
        ▼
McpMainThreadDispatcher
        │
        ▼
 permission policy
        │
   ┌────┴────┐
   ▼         ▼
Resources   Tools
   │         │
   │         ▼
   │    Scene Commands
   │         │
   └────┬────┘
        ▼
   ProjectSession
   ├── EditorScene
   ├── ReflectionRegistry
   ├── AssetRegistry
   └── CommandBus
```

首批 Resources：

```text
engine://project/info
engine://scene/current
engine://scene/hierarchy
engine://entity/{uuid}
engine://asset/{uuid}
```

首批 Tools：

```text
scene.create_entity
scene.delete_entity
scene.rename_entity
scene.add_component
scene.remove_component
scene.set_component_property
scene.save
```

`scene.set_component_property` 的 schema 与 typed JSON 转换来自 Reflection metadata；MCP 不维护第二份组件属性表。所有作者态 mutation 在 Editor 主线程进入同一个 `ProjectSession::CommandBus`，因此 Agent 修改会标记 Scene dirty，并可被 Human 的 Undo/Redo 历史撤销或重做。Play Mode 期间写工具遵循与 Human 相同的作者态只读规则；读取资源仍描述 EditorScene，而不是 RuntimeScene。

权限层当前把操作显式分类为 ProjectRead、SceneRead、SceneWrite、SceneSave。默认本地 `JanusEditor --mcp-stdio` 使用 allow-all policy，但授权 seam 已独立于 Tool/Resource handler，后续可以替换策略而不改业务能力。

以下为 v0.8 发布时的边界；Runtime/诊断/Transaction/Activity 已由上述 v0.9 扩展：

- 不包含 Streamable HTTP / OAuth / remote transport；
- 不包含 runtime.play / pause / stop / step；
- 不包含 Runtime Scene resource、Profiler、structured runtime logs；
- 不包含 Transaction / grouped command / Audit / Agent Activity；
- 不暴露任意 shell、文件系统或网络能力；
- 不新增 MCP 专用 Undo API，Human 与 Agent 继续共享同一个 CommandBus history。

自动化验证分成两层：live Editor integration 测试验证 `McpEditorHost + ProjectSession` 的主线程、dirty、Play 与 Human Undo 语义；独立子进程 stdio E2E 验证真实进程边界、modern/legacy 协议、Resources/Tools、Scene Save 与 stdout purity。这样协议 CI 不依赖 hosted runner 的 GPU/OpenGL 驱动，同时两层都复用生产的 MCP/ProjectSession capability graph。

## v0.7 Reflection + Command 工作流

v0.7 将 v0.6 的临时 authoring seam 收敛为可同时服务 Human Editor 与未来 MCP 的 Engine Capability：

```text
Inspector / Asset Browser / Hierarchy
                │
                ▼
           EditorActions
                │
                ▼
ProjectSession::CommandBus
                │
        ┌───────┴────────┐
        ▼                ▼
 Scene Commands     SceneReflection
                         │
                         ▼
                 ReflectionRegistry
```

`ReflectionRegistry` 为 Transform、SpriteRenderer、Camera、LuaScript 提供稳定的组件/属性 metadata。Inspector 根据 `ComponentDescriptor / PropertyDescriptor / PropertyType` 枚举并生成当前内置组件编辑 UI，不再维护一份 Transform/SpriteRenderer/Camera/LuaScript 的硬编码属性列表。runtime-only Transform cache/dirty、Hierarchy internals 等状态不会进入 authoring metadata。

Scene v1 的 JSON 结构保持兼容，但组件与属性持久化已经改为 Reflection 驱动。SceneSerializer / SceneDeserializer / SceneCloner 都显式接收 host 持有的 ReflectionRegistry；ProjectSession 与 managed Runtime Application 各自拥有明确生命周期的 registry，不使用全局 singleton 或每次调用临时构造的 metadata universe。

作者态修改统一进入 CommandBus。当前支持：

```text
SetPropertyCommand
AddComponentCommand
RemoveComponentCommand
CreateEntityCommand
DeleteEntityCommand
RenameEntityCommand
```

Delete Undo 使用 persistent UUID + reflected authoring snapshot 重建实体子树，不依赖 ECS index/generation；Camera.primary 这类跨实体副作用通过 PropertyMutationDelta 一并记录，因此 Undo/Redo 可以恢复原来的主 Camera。Play Mode 只运行隔离的 RuntimeScene，作者态 Execute / Undo / Redo 会被拒绝，Stop 后 EditorScene 与 authoring history 保留。

当前 v0.7 的明确边界：

- 通用 Inspector 的 AssetReference 展示 UUID 与类型约束；Texture / LuaScript / Font 的类型安全赋值由 Asset Browser 完成；
- 通用 Inspector 一次提交一个 reflected property；为了不提前引入 Transaction，旧的多字段兼容 helper 可能对应多个 history entry；
- Command history 只属于当前 ProjectSession / authoring document；
- 不包含 MCP transport/JSON-RPC、Transaction、Audit、Agent Activity、Profiler 或 user-facing Reparent workflow，这些属于后续里程碑。

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

v0.6 的 Inspector mutation 被集中在 `EditorActions` seam；v0.7 已将其内部实现替换为 Reflection + CommandBus，并在保留 Editor panel 分层的同时加入通用属性 metadata 与 Undo/Redo。

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
├── SandboxProject/  v0.8 Editor / MCP / Reflection / Lua / Asset workflow fixture
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

## 10-05a Button 计数菜单（本地实现）

`SandboxProject/Scenes/ButtonShowcase.scene` 演示两个计数按钮、禁用状态与 Lua `OnClick(self)`。在项目副本的 `project.json` 中将 `defaultScene` 设为 `Scenes/ButtonShowcase.scene`，用 `JanusSandbox.exe <项目副本路径>` 或 `JanusEditor.exe --project <项目副本路径>` 打开。Editor 在 Game View 中 Play 后点击；Up/Down 选按钮，Enter/Space 确认，拖出取消，Stop 恢复作者态计数。Button 的启用状态和四种颜色可在 Inspector 编辑并撤销；相同字段也支持 MCP 场景工具。

这完成 10-05a 点击计数切片；10-05b 的后续实现见下节。见[设计](docs/superpowers/specs/2026-09-07-v0.10-ui-button-design.md)与[验收](docs/verification/2026-09-07-v0.10-ui-button.md)。


## 10-05b 固定卡牌战斗（本地实现）

[Game/](Game/README.md) 已提供菜单、选牌/出牌、血量、胜负与重开。构建后运行 `JanusSandbox.exe ./Game` 或 `JanusEditor.exe --project ./Game`；Editor 在 Game View 中 Play。三次 Strike 胜利，三次 Wait 失败；鼠标和 Up/Down + Enter/Space 共用 Button 规则。

Agent 可读 `engine://runtime/snapshot` 获取阶段、血量、伤害、回合与运行身份。验证场景和 modern/legacy 回归样例见 [Game 说明](Game/README.md)，本次证据见[验收记录](docs/verification/2026-09-07-v0.10-playable-combat.md)。

## 10-06 Sprite 动画（本地实现）

AnimationClip/Animator 已接入共享 Runtime，支持帧时长、循环、Play/Stop/Switch、暂停单步和 Sprite/Image 帧覆盖。战斗成功出牌会播放一次边框动画；`Game/Scenes/AnimationShowcase.scene` 演示循环机器人、Space 停止、Enter 重播和 D 切换一次播放。运行与 Agent 编辑方法见 [Game 说明](Game/README.md)，边界和证据见[设计](docs/superpowers/specs/2026-09-07-v0.10-animation-design.md)、[验收](docs/verification/2026-09-07-v0.10-animation.md)。10-05b 与 10-06 已拆为独立提交和依赖式 PR（战斗基于 Text，动画基于战斗）；下一包为 10-07 Audio，v0.10 尚未整体完成。
