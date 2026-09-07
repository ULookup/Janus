# Janus Engine 技术架构设计

## 1. 文档目标

本文档定义 Janus Engine 的整体技术架构、模块边界、依赖关系、核心运行时模型以及 Agent/MCP 接入方式。

核心目标不是描述某个类怎么写，而是回答：

- 引擎由哪些核心模块组成；
- 模块之间允许如何依赖；
- Editor、Game、MCP 如何共享 Engine Capability；
- Runtime 数据如何流转；
- Scene、ECS、Renderer、Asset、Lua 如何解耦；
- MCP 如何成为原生能力，而不是外挂插件；
- 哪些能力必须预留扩展点；
- 哪些复杂度明确不进入早期版本。

---

# 2. 核心架构原则

Janus Engine 遵循以下原则。

## 2.1 分层而非全局 Manager

禁止形成：

```text
GameManager
SceneManager
AssetManager
RenderManager
MCPManager
...
```

互相直接引用的架构。

所有模块必须拥有明确职责和依赖方向。

---

## 2.2 上层不感知底层实现

例如：

```text
Game
```

不得感知：

```text
OpenGL
SDL
Box2D
miniaudio
```

正确关系：

```text
Game
 ↓
Engine API
 ↓
Engine Service
 ↓
Backend
```

---

## 2.3 Editor、Game、Agent 都是 Engine Client

核心模型：

```text
                   Janus Engine
                        ▲
        ┌───────────────┼───────────────┐
        │               │               │
      Game            Editor           MCP
```

不能设计成：

```text
MCP
 ↓
Editor
 ↓
Engine
```

Agent 应直接操作 Engine Capability。

---

## 2.4 数据与行为分离

Scene World 采用：

```text
Entity
+
Component
+
System
```

模式。

Component 尽量只保存数据。

System 负责逻辑。

---

## 2.5 所有编辑操作通过 Command

Editor 和 Agent 不直接修改 Scene 内部状态。

统一：

```text
Editor ──┐
         ├──► CommandBus ──► Engine
MCP ─────┘
```

以支持：

- Undo；
- Redo；
- Audit；
- Transaction；
- Agent 操作历史。

---

## 2.6 Reflection 作为元数据核心

Reflection 同时服务：

```text
Inspector
Serialization
MCP Schema
Debug
```

避免同一 Component 在四个系统重复注册字段。

---

# 3. 总体架构

```text
┌──────────────────────────────────────────────────────────────┐
│                         Game Layer                           │
│                                                              │
│     C++ Gameplay / Lua Gameplay / Game UI / Network Logic   │
└───────────────────────────┬──────────────────────────────────┘
                            │
                    Engine Public API
                            │
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                     Runtime Framework                        │
│                                                              │
│   Scene      ECS      Script      Animation      Game UI     │
│                                                              │
│   Input      Physics   Audio       Network                   │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                       Engine Services                        │
│                                                              │
│ RenderingServer        AssetService        Event/Command     │
│                                                              │
│ RuntimeService         Reflection          Serialization     │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                     Hardware Abstraction                     │
│                                                              │
│ RenderDevice   Window   FileSystem   AudioDevice   Socket    │
└───────────────────────────┬──────────────────────────────────┘
                            │
            ┌───────────────┼───────────────────┐
            ▼               ▼                   ▼
         OpenGL            SDL3                OS
```

旁路：

```text
┌──────────────────────┐       ┌──────────────────────┐
│       Editor         │       │     MCP Gateway      │
│                      │       │                      │
│ Hierarchy            │       │ Tools                │
│ Inspector            │       │ Resources            │
│ Scene View           │       │ Tasks                │
│ Asset Browser        │       │ Permission           │
│ Profiler             │       │ Audit                │
└──────────┬───────────┘       └──────────┬───────────┘
           │                              │
           └──────────────┬───────────────┘
                          ▼
                     CommandBus
```

---

# 4. 工程目录设计

```text
Janus/
│
├── Engine/
│   │
│   ├── Core/
│   │   ├── Application/
│   │   ├── Base/
│   │   ├── Log/
│   │   ├── Time/
│   │   ├── UUID/
│   │   ├── Memory/
│   │   ├── Thread/
│   │   ├── Event/
│   │   ├── Command/
│   │   ├── Reflection/
│   │   └── FileSystem/
│   │
│   ├── Platform/
│   │   ├── Window/
│   │   ├── Input/
│   │   └── SDL/
│   │
│   ├── Renderer/
│   │   ├── RenderDevice/
│   │   ├── RenderServer/
│   │   ├── RenderCommand/
│   │   ├── RenderQueue/
│   │   ├── Renderer2D/
│   │   ├── Buffer/
│   │   ├── Texture/
│   │   ├── Shader/
│   │   ├── Material/
│   │   ├── Framebuffer/
│   │   └── Camera/
│   │
│   ├── Backend/
│   │   └── OpenGL/
│   │
│   ├── ECS/
│   │   ├── Entity/
│   │   ├── EntityManager/
│   │   ├── SparseSet/
│   │   ├── ComponentPool/
│   │   ├── Registry/
│   │   └── View/
│   │
│   ├── Scene/
│   │   ├── Scene/
│   │   ├── Hierarchy/
│   │   ├── Transform/
│   │   ├── Prefab/
│   │   └── SceneSerializer/
│   │
│   ├── Asset/
│   │   ├── Asset/
│   │   ├── AssetHandle/
│   │   ├── AssetRegistry/
│   │   ├── AssetLoader/
│   │   ├── AssetImporter/
│   │   ├── AssetCache/
│   │   └── AssetService/
│   │
│   ├── Script/
│   │   ├── LuaVM/
│   │   ├── LuaBinding/
│   │   ├── ScriptEngine/
│   │   ├── ScriptInstance/
│   │   └── HotReload/
│   │
│   ├── Animation/
│   │
│   ├── Physics/
│   │
│   ├── Audio/
│   │
│   ├── UI/
│   │
│   ├── Network/
│   │
│   └── Debug/
│       ├── Profiler/
│       ├── Statistics/
│       └── Instrumentation/
│
├── Editor/
│   ├── EditorApp/
│   ├── Hierarchy/
│   ├── Inspector/
│   ├── SceneView/
│   ├── GameView/
│   ├── AssetBrowser/
│   ├── Console/
│   ├── Profiler/
│   └── AgentActivity/
│
├── MCP/
│   ├── Protocol/
│   ├── Transport/
│   ├── ToolRegistry/
│   ├── ResourceRegistry/
│   ├── TaskRegistry/
│   ├── Permission/
│   ├── Transaction/
│   └── Audit/
│
├── Runtime/
│
├── Sandbox/
│
├── Tests/
│
├── Benchmarks/
│
└── Game/
```

---

# 5. Core 模块

Core 是整个引擎最底层公共能力。

Core 不允许依赖：

```text
Renderer
Scene
Asset
Editor
MCP
Game
```

主要职责：

```text
Application Lifecycle
Logging
Time
UUID
Memory Utilities
Thread Utilities
FileSystem Abstraction
Event
Command
Reflection
Assertions
Result/Error
```

---

# 6. Application 与主循环

Application 负责整个 Runtime 生命周期。

```text
Application
    │
    ├── Initialize
    │
    ├── MainLoop
    │
    └── Shutdown
```

主循环：

```text
Poll Platform Events
        ↓
Input Update
        ↓
Variable Update
        ↓
Fixed Update
        ↓
Animation
        ↓
Physics
        ↓
Transform Propagation
        ↓
Render Submission
        ↓
Render
        ↓
Present
```

必须避免 System 自己决定执行时机。

后期所有 Runtime System 均进入统一 Scheduler。

首期可使用固定顺序。

10-03 实现细化：managed Application 与 RuntimeSession 已通过 `Engine/Runtime/RuntimeExecution` 共享现有脚本执行阶段（输入快照 → 可选热重载 → Script Update）。Scene 所有权、Editor Clone/Faulted 状态机和渲染仍由宿主管理；Paused Step 保持中性输入、1/60 秒和不重载。上图的 Fixed Update/Animation/Physics 是后续目标流程，尚未因本次抽取而实现；不引入空系统或动态 Scheduler。生命周期和验证见[共享 Runtime 设计](superpowers/specs/2026-09-07-v0.10-shared-runtime-design.md)及[实施记录](verification/2026-09-07-v0.10-shared-runtime.md)。

---

# 7. ECS 架构

Janus 首期采用自研轻量 ECS。

## Entity

Entity 不保存业务对象本身。

设计：

```text
Entity
=
Index
+
Generation
```

Generation 防止 Entity ID 被复用以后旧引用错误命中新对象。

---

## Component

Component 为纯数据结构。

例如：

```text
TransformComponent

position
rotation
scale
```

原则：

- 不拥有 Update；
- 不直接访问 Scene；
- 不直接发网络；
- 不直接执行 Renderer；
- 尽量 POD / trivially movable。

---

## Component Storage

首期核心结构：

```text
Sparse Set
+
Dense Component Array
```

目标：

- Entity → Component 查询接近 O(1)；
- 同类 Component 连续存储；
- 遍历时具有较好缓存局部性。

---

## Registry

Registry 提供：

```text
CreateEntity

DestroyEntity

AddComponent<T>

RemoveComponent<T>

GetComponent<T>

HasComponent<T>

View<T...>
```

---

# 8. Scene 架构

Scene 是一个独立游戏世界。

```text
Scene
│
├── Registry
├── Hierarchy
├── RuntimeState
└── SceneMetadata
```

Scene 管：

- Entity 生命周期；
- Hierarchy；
- Runtime 初始化；
- Scene Serialize；
- Scene Play Copy。

---

# 9. Scene Hierarchy

ECS 本身没有树结构。

Janus 使用 RelationshipComponent：

```text
parent
firstChild
nextSibling
previousSibling
```

或者等价索引结构。

用于：

```text
Scene
├── Player
│   ├── Weapon
│   └── Effect
└── Enemy
```

Transform 依赖 Hierarchy 进行世界矩阵传播。

---

# 10. Transform 系统

每个 Entity 默认具有 TransformComponent。

核心数据：

```text
Local Position
Local Rotation
Local Scale

World Transform Cache
Dirty Flag
```

父节点发生变化后：

```text
Mark Dirty
 ↓
Child Dirty Propagation
 ↓
Lazy / Scheduled Recompute
```

避免每帧无条件重算所有节点。

---

# 11. Renderer 分层

Renderer 是核心技术模块，必须严格分层。

```text
Scene / Game
     │
     ▼
RenderSystem
     │
     ▼
RenderServer
     │
     ▼
RenderQueue
     │
     ▼
Renderer2D
     │
     ▼
RenderDevice
     │
     ▼
OpenGLRenderDevice
```

Game、Scene 禁止直接调用 OpenGL。

---

# 12. RenderDevice

RenderDevice 是图形 API 抽象层。

能力：

```text
CreateBuffer
CreateTexture
CreateShader
CreateFramebuffer

UpdateBuffer

BindPipeline State

DrawIndexed

SetViewport

Clear
```

首期只有：

```text
OpenGLRenderDevice
```

但接口不写死 OpenGL 类型。

禁止向上层暴露：

```text
GLuint
GLenum
gl*
```

---

# 13. Render Resource Handle

Renderer 上层不直接保存 GPU 对象。

例如：

```text
TextureHandle
BufferHandle
ShaderHandle
FramebufferHandle
```

映射：

```text
TextureHandle
     ↓
RenderDevice Resource Table
     ↓
OpenGL Texture
```

目的：

- 隔离后端；
- 生命周期可控；
- Debug 可追踪；
- 支持未来资源重建。

---

# 14. Renderer2D

首期重点能力：

```text
Sprite
Texture
Color
Transform
Layer
Order
Camera
```

流程：

```text
Scene
 ↓
Sprite Submission
 ↓
RenderQueue
 ↓
Sort
 ↓
Batch
 ↓
Vertex Buffer
 ↓
DrawIndexed
```

---

# 15. Batch Renderer

Batch Renderer 是首个重点性能模块。

尽量把：

```text
Sprite × N
```

合并为更少 Draw Call。

Batch Key 可由：

```text
Layer
Material
Shader
Texture Set
Blend Mode
```

组成。

需要统计：

```text
Sprite Count
Batch Count
Draw Calls
Texture Bind Count
Vertex Count
Index Count
```

---

# 16. Framebuffer

Scene View 和 Game View 使用 Framebuffer。

```text
Runtime Renderer
      ↓
Framebuffer
      ↓
Color Texture
      ↓
Editor Game View
```

Scene View 使用独立 Editor Camera。

两者不得共用同一个 Camera 状态。

---

# 17. Asset 架构

Asset System 不只是文件加载器。

核心：

```text
AssetHandle
AssetMetadata
AssetRegistry
AssetImporter
AssetLoader
AssetCache
```

资源引用统一使用：

```text
AssetHandle
```

Scene 文件不得长期直接持有绝对路径。

---

# 18. Asset 生命周期

典型流程：

```text
Assets/player.png
        ↓
AssetImporter
        ↓
AssetMetadata
        ↓
AssetRegistry
        ↓
AssetHandle
        ↓
AssetService::Load
        ↓
Cache Lookup
   ┌────┴────┐
   ▼         ▼
Hit         Miss
             ↓
           Loader
             ↓
        Runtime Asset
```

GPU 资源继续通过 RenderDevice 创建。

---

# 19. Asset Metadata

每个 Asset 至少包含：

```text
Handle
Type
Source Path
Import Settings
Dependencies
Version
```

用于：

- Editor；
- MCP；
- Reimport；
- Dependency Query；
- Serialization。

---

# 20. Reflection

Reflection 是核心横向基础设施。

类型元数据：

```text
Type
├── Name
├── ID
├── Properties
│   ├── Name
│   ├── Type
│   ├── Offset / Accessor
│   └── Attributes
└── Constructor / Serializer Hooks
```

主要消费者：

```text
Inspector
SceneSerializer
MCP Schema
Debug Dump
```

---

# 21. Serialization

Scene Serialization 依赖 Reflection。

序列化内容：

```text
Scene Metadata
Entity UUID
Entity Name
Hierarchy
Components
Component Properties
AssetHandle
```

保存要求：

```text
Write Temporary File
        ↓
Validate
        ↓
Atomic Replace
```

降低工程损坏风险。

---

# 22. Lua Runtime

Lua 用于 Gameplay，而非替代 Engine Core。

架构：

```text
Lua VM
  │
  ▼
ScriptEngine
  │
  ▼
ScriptInstance
  │
  ▼
Entity
```

生命周期：

```text
OnCreate
OnUpdate
OnDestroy
```

Engine API 通过 Binding 暴露。

Lua 不允许直接获得 ECS Registry 原始指针。

---

# 23. Lua Object Lifetime

C++ 对象为真实所有者。

Lua 保存：

```text
EntityHandle
AssetHandle
Safe Native Reference
```

而不是裸指针。

当 Entity 被销毁：

```text
Lua Reference
 ↓
Validation Fails
 ↓
Controlled Error
```

避免悬空对象。

---

# 24. Lua Hot Reload

首期支持：

```text
File Changed
 ↓
Script Reload
 ↓
Replace Function Table
 ↓
Rebind Instance
```

早期不保证所有 Lua VM State 完整迁移。

重点先保证：

```text
Engine 不重启
Scene 不重载
Gameplay 可以快速迭代
```

---

# 25. Physics

Physics 首期包装 Box2D。

架构：

```text
Game
 ↓
Janus Physics API
 ↓
PhysicsService
 ↓
Box2D
```

Game 不允许持有 Box2D 原生对象。

首期提供：

```text
RigidBody2D
Collider2D
Trigger
Raycast
Collision Event
```

---

# 26. Audio

首期通过 miniaudio 等成熟库实现。

Janus API：

```text
AudioClip
AudioSource
Play
Pause
Stop
Volume
Loop
```

不追求复杂 DSP。

---

# 27. Input

底层：

```text
SDL Event
```

上层：

```text
Input State
 ↓
Input Mapping
 ↓
Action
```

Gameplay 使用：

```text
MoveUp
Confirm
Cancel
```

而不是直接依赖键值。

---

# 28. Editor 架构

Editor 不是 Engine Core。

```text
JanusEditor
    │
    ▼
Engine Public API
```

主要模块：

```text
Hierarchy
Inspector
Scene View
Game View
Asset Browser
Console
Profiler
Agent Activity
```

第一阶段基于 Dear ImGui。

---

# 29. Editor 与 Runtime Scene

进入 Play：

```text
EditorScene
    │
Clone
    ▼
RuntimeScene
```

运行期间所有 Gameplay 修改作用于 RuntimeScene。

Stop：

```text
Destroy RuntimeScene
Return EditorScene
```

首期不实现 Play Mode 修改自动 Apply。

---

# 30. CommandBus

所有可编辑行为必须命令化。

接口概念：

```text
ICommand

Validate
Execute
Undo
Redo
Describe
```

典型 Command：

```text
CreateEntityCommand
DeleteEntityCommand
RenameEntityCommand
ReparentEntityCommand
AddComponentCommand
RemoveComponentCommand
SetPropertyCommand
```

---

# 31. Command History

结构：

```text
CommandBus
    │
    ├── Execute
    │
    ├── UndoStack
    │
    └── RedoStack
```

Agent 与 Editor 使用同一个 History。

这样用户可 Undo Agent 行为。

---

# 32. Transaction

Agent 批量操作必须支持 Transaction。

v0.9 的实施细化方案见 [Agent Development Loop 设计](superpowers/specs/2026-09-06-v0.9-agent-development-loop-design.md)。下方 `Validate All` 是批处理概念流程，不意味着 Begin 时能够校验尚未收到、或依赖前序 Create 的命令。v0.9 采用会话级独占写入、逐命令校验、临时执行、Commit 分组历史及逆序补偿；临时状态对读取可见，回滚失败进入显式 RecoveryRequired。Save/Runtime 不属于作者态事务，也不承诺磁盘原子提交或数据库读隔离。本地实现及验证证据见 [v0.9 验证记录](verification/2026-09-06-v0.9-agent-development-loop.md)，集成/发布状态与本地完成状态分开记录。

```text
Transaction
│
├── Command A
├── Command B
├── Command C
└── Command D
```

流程：

```text
Validate All
 ↓
Execute
 ↓
Failure?
 ├── No → Commit
 └── Yes → Rollback
```

---

# 33. MCP 总体架构

```text
External Agent
      │
      ▼
MCP Transport
      │
      ▼
MCP Protocol
      │
      ▼
Tool / Resource / Task Registry
      │
      ▼
Agent Capability Layer
      │
      ├── Query
      └── Command
             │
             ▼
          CommandBus
             │
             ▼
           Engine
```

MCP 永远不直接修改 ECS、Asset 内部容器。

---

# 34. MCP Transport

抽象：

```text
IMcpTransport
```

首期：

```text
StdioTransport
```

后续：

```text
HttpTransport
```

Transport 不包含 Engine Logic。

---

# 35. MCP Tool Registry

工具不应该手写成大量重复函数。

通用操作：

```text
scene.create_entity
scene.add_component
scene.set_component_property
```

配合 Reflection 支持任意已注册 Component。

---

# 36. MCP Resource Registry

只读资源：

```text
engine://project/info

engine://scene/current

engine://scene/hierarchy

engine://entity/{id}

engine://asset/{id}

engine://runtime/status

engine://profiler/latest-frame

engine://logs/recent
```

Resource Query 不产生副作用。

---

# 37. MCP Thread Model

MCP IO 不直接运行于 Engine Main Thread。

```text
MCP IO Thread
     │
     ▼
Parse Request
     │
     ▼
Query / Command Queue
     │
══════════════════════
     │ Main Thread
     ▼
Execute Engine Work
     │
     ▼
Result
     │
══════════════════════
     ▼
MCP Response
```

所有 Scene/ECS 修改在 Main Thread 完成。

---

# 38. MCP 权限

Capability 基础等级：

```text
ReadOnly
Editor
Developer
Elevated
```

Capability 采用白名单。

默认不暴露：

```text
Arbitrary Shell
Raw Filesystem
Native DLL Load
Raw Memory Access
```

---

# 39. Audit

所有 Agent 修改记录：

```text
Timestamp
Client
Tool
Command
Arguments Summary
Result
Affected Objects
```

Editor 提供 Agent Activity 面板。

---

# 40. Profiler

Profiler 分为：

```text
CPU Instrumentation

Renderer Statistics
```

CPU 使用：

```text
PROFILE_SCOPE
```

采集：

```text
Thread
Start Time
Duration
Parent Scope
```

Renderer：

```text
Draw Calls
Batch Count
Sprite Count
Texture Binds
```

---

# 41. Profiler 数据消费者

同一份 Profiler Data：

```text
Profiler Data
     │
     ├── Editor Profiler
     ├── MCP Resource
     └── Benchmark
```

禁止 Editor Profiler 单独维护另一套统计。

---

# 42. Testing

测试分层：

```text
Unit Test
Integration Test
Engine Runtime Test
Scene Test
Benchmark
```

重点模块必须具备单元测试：

```text
Entity Generation
SparseSet
Registry
Serialization
Asset Registry
Command Undo/Redo
Transaction
Reflection
```

---

# 43. Benchmark

独立：

```text
Benchmarks/
```

重点：

```text
ECS iteration
ECS create/destroy
Renderer sprite batch
Asset cache
Serialization
```

Benchmark 数据不作为 API 契约，但用于检测性能退化。

---

# 44. Network

Engine Network 只提供通用传输基础：

```text
Connection
Packet
Protocol Utilities
Session
```

具体：

```text
Login
Battle
Matchmaking
CardPlay
```

属于 Game。

对于卡牌 Demo：

```text
Game Client
     │
Command
     ▼
Authoritative Server
     │
Event
     ▼
Game Client
```

---

# 45. Headless Mode

最终 Application 支持：

```text
Editor Mode

Runtime Mode

Headless Mode
```

三种模式共享 Engine Core。

Headless 不初始化：

```text
Editor UI
Window
Interactive Input
```

但可初始化：

```text
Scene
Asset
Lua
Test
MCP
```

必要时未来支持 Headless Rendering。

---

# 46. 依赖规则

允许：

```text
Game → Engine

Editor → Engine

MCP → Engine Capability

Scene → ECS

Scene → Renderer API

Renderer → RenderDevice

OpenGL Backend → OpenGL
```

禁止：

```text
Engine → Game

Scene → OpenGL

Game → OpenGL

Game → Box2D

MCP → ECS internal storage

Editor → ECS internal storage

Asset → Editor

Renderer → Editor
```

---

# 47. 第三方依赖边界

允许采用成熟库：

```text
SDL3

OpenGL Loader

glm

stb_image

Lua

Box2D

miniaudio

Dear ImGui

GoogleTest / Catch2
```

第三方库必须尽量封装于 Janus API 后方。

例如：

```text
Game
  X
Box2D
```

必须经过：

```text
Janus Physics
```

---

# 48. 错误模型

核心函数尽量避免异常作为业务控制流。

采用：

```text
Result<T, Error>
```

或等价显式错误模型。

错误至少包含：

```text
ErrorCode
Message
Context
```

MCP Error 由 Engine Error 映射，而不是 Engine 内部直接构造 MCP JSON。

---

# 49. ID 模型

不同 ID 明确区分：

```text
EntityID
EntityUUID

AssetHandle

RenderResourceHandle

CommandID

TransactionID
```

避免所有东西都使用裸 uint64_t 后失去类型安全。

---

# 50. 架构演进预留

首期不实现，但保持边界：

```text
Vulkan Backend
3D Renderer
Job System
Async Asset Loading
Prefab Override
Render Graph
GPU Profiler
Remote MCP
Plugin System
```

禁止为了这些未来需求提前增加大量复杂度。

---

# 51. 架构成功标准

技术架构成功不以类数量判断。

必须满足：

1. Engine 可独立运行；
2. Game 不依赖具体图形 API；
3. Editor 不直接修改 ECS internals；
4. MCP 不直接修改 ECS internals；
5. Scene 可序列化；
6. Resource 通过 Handle 引用；
7. Lua 不持有危险裸指针；
8. Agent 操作可 Undo；
9. Agent 批量操作可 Transaction；
10. Profiler 数据可被 Editor 和 Agent 同时消费；
11. 真实 Demo Game 可以基于 Engine Public API 完成；
12. 核心模块存在自动测试和 Benchmark。

---

# 52. 最终技术形态

Janus Engine 的核心不是：

```text
OpenGL Renderer
+
ECS
+
MCP Server
```

而是：

```text
                           Janus
                             │
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
            Human          Agent           Game
              │              │              │
           Editor           MCP          C++ / Lua
              │              │              │
              └──────────────┼──────────────┘
                             ▼
                     Engine Capability
                             │
                  Command / Reflection
                             │
           ┌─────────────────┼─────────────────┐
           ▼                 ▼                 ▼
         Scene             Asset            Runtime
           │                 │                 │
          ECS              Cache           Profiler
           │                 │                 │
           └─────────────────┼─────────────────┘
                             ▼
                      Engine Services
                             │
                             ▼
                     Platform / GPU / OS
```

Janus 的架构价值在于：

> **Human、Agent 和 Game 都通过稳定的 Engine Capability 与同一个游戏世界交互。**

## v0.10 / 10-04a UI 布局实现补充

Engine/UI 持有可反射的 Canvas/UIRect/Panel/Image 作者态定义，UILayout 从 Scene 层级生成共享绘制与几何命中顺序。SceneRenderer 将 UI 作为世界 Sprite 后的屏幕空间阶段，使用项目逻辑分辨率、CPU 矩形/UV 裁剪和只合并相邻图元的批处理。ReparentEntityCommand 供 EditorActions 与 MCP 共用，保留局部字段及可撤销兄弟顺序。详见[专项设计](superpowers/specs/2026-09-07-v0.10-ui-layout-design.md)和[本地验收](verification/2026-09-07-v0.10-ui-layout.md)；#84 已合入 shared-runtime 父分支，尚未进入 `c71ad50` main。Text 属于下述 10-04b，Button 与后续游戏系统不在布局包。

## v0.10 / 10-04b Text 实现补充

Asset 增加 Font v1 离线图集/Unicode 字形度量，CPU 缓存复用 AssetCache，atlas 纹理由既有 AssetService/Renderer2D 管理。TextLayout 负责有界 UTF-8、换行、按行对齐和矩形/UV 裁剪，SceneRenderer 在稳定 UI 顺序中提交字形。Text 贯通 Reflection/Scene v1/Clone/Command/Inspector/MCP；Lua get_text/set_text 通过既有共享 RuntimeExecution 修改运行 Scene。AssetRegistry::Search 与 ProjectRead 分类的 MCP assets.search 提供稳定、有界的只读资产发现。详见[专项设计](superpowers/specs/2026-09-07-v0.10-ui-text-design.md)及[本地验收](verification/2026-09-07-v0.10-ui-text.md)。不新增运行时字体依赖；Button、事件消费和单局玩法仍属于 10-05。

## v0.10 / 10-05a Button 实现补充

RuntimeExecution 拥有 UIInteraction 的临时焦点/捕获状态，布局收集 UUID 事件后先消费输入，再通过 ScriptEngine 调用同实体固定 OnClick，随后执行 OnUpdate。Button 作者态仍通过 active Reflection、Scene v1、Clone、共享 Command、Inspector/MCP；Renderer 只读交互状态生成按钮颜色，Core 仅提供有界事件记录及通用输入过滤。Start/Resume 不重放旧输入，Paused Step 不派发 UI。详见[设计](superpowers/specs/2026-09-07-v0.10-ui-button-design.md)和[验收](verification/2026-09-07-v0.10-ui-button.md)。10-05b 玩法与结构化快照尚未实现。


## v0.10 / 10-05b Combat 与诊断快照实现补充

Game/ 是复用共享 Runtime 和既有 UI 的独立磁盘游戏项目，固定卡牌规则完全位于 Lua。ScriptEngine 缓存脚本显式发布的有界平坦标量映射，RuntimeExecution 赋运行 UUID/发布帧，RuntimeSession 使用同一身份；Application/RuntimeSession 暴露相同只读快照。MCP 的 engine://runtime/snapshot 经 RuntimeRead 和主线程权限链读取缓存，不调用 Lua。Stop 清理，Faulted 保留最后原子发布及失败帧状态；不改 Scene 作者态或 neutral Step。见[设计](superpowers/specs/2026-09-07-v0.10-playable-combat-design.md)和[验收](verification/2026-09-07-v0.10-playable-combat.md)。上方 10-05b 尚未实现的表述为历史状态；Animation/Audio/Physics/Prefab 仍未由本包实现。
