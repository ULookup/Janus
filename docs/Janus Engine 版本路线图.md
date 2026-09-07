# Janus Engine 版本路线图

当前执行状态（2026-09-07）：main / origin/main 为 `7ecdfb8`，10-01～10-10 的计划切片与综合示例均已集成；[PR #92](https://github.com/ULookup/Janus/pull/92) 已合并，合并后 Windows CI 412/412 通过。**v0.10 处于发布准备阶段，尚未发布；v0.11 Production Demo 尚未完成。** 当前实现证据、Human 制作入口缺口、PRD 对账及发布门槛统一见[项目进度总表](project-status.md)。详细版本范围仍由本文定义；日期化计划、验收及文末执行摘录保留历史语境，不作为当前 main 状态。

## 1. 路线图目标

Janus Engine 采用：

> **纵向闭环优先于横向功能数量**

的研发策略。

不按照：

```text
先把 Renderer 全做完
再把 ECS 全做完
再把 Editor 全做完
```

无限扩张。

而是逐阶段构建：

```text
Runtime
↓
World
↓
Content
↓
Editor
↓
Agent
↓
Validation
```

最终形成完整游戏开发闭环。

---

# 2. 版本总览

```text
v0.1
Engine Foundation
      ↓
v0.2
Renderer2D
      ↓
v0.3
ECS + Scene
      ↓
v0.4
Asset + Serialization
      ↓
v0.5
Lua Gameplay Runtime
      ↓
v0.6
Editor Foundation
      ↓
v0.7
Command + Reflection
      ↓
v0.8
MCP Agent Foundation
      ↓
v0.9
Profiler + Agent Loop
      ↓
v0.10
Game Systems
      ↓
v0.11
Production Demo
      ↓
v1.0
Janus Engine
```

---

# 3. v0.1 — Engine Foundation

状态：已完成

## 目标

建立稳定、可测试、可持续扩展的 C++ Engine Skeleton。

## 范围

```text
CMake

Engine / Sandbox 分离

Application

MainLoop

SDL3 Window

OpenGL Context

Logging

Assertion

Timer

Basic Event

FileSystem Utility

Unit Test Framework
```

## Demo

启动：

```text
Janus Sandbox
```

显示空窗口。

支持：

```text
Close
Resize
Keyboard Input
```

## 核心知识

```text
CMake

RAII

Application Lifecycle

Event Loop

Delta Time

Platform Abstraction
```

## 验收

- Sandbox 独立链接 Engine；
- Window 可稳定运行；
- Engine 无 Gameplay 代码；
- Core 不依赖 Renderer；
- 基础 Unit Test 可执行。

---

# 4. v0.2 — Renderer2D

状态：已完成

## 目标

打通完整 CPU → GPU 2D Rendering Pipeline。

## 范围

```text
RenderDevice

OpenGLRenderDevice

VertexBuffer

IndexBuffer

VertexArray abstraction

Shader

Texture

Framebuffer

Orthographic Camera

Sprite Renderer

Render Queue

Batch Renderer

Renderer Statistics
```

## Demo

屏幕展示：

```text
多 Texture Sprite

移动 Camera

透明 Sprite

1000+ Sprite 测试
```

## Benchmark

至少记录：

```text
1K Sprite

10K Sprite
```

对应：

```text
Frame Time
Draw Calls
Batch Count
Sprite Count
```

## 核心知识

```text
GPU Pipeline

OpenGL

VBO / EBO

Shader

Texture

Framebuffer

Alpha Blend

Draw Call

Batching
```

## 验收

Game/Sandbox 不出现：

```text
gl*
GLuint
```

调用。

---

# 5. v0.3 — ECS + Scene

状态：已完成

## 目标

建立完整 Game World Runtime。

## 范围

```text
Entity

Index + Generation

SparseSet

ComponentPool

Registry

View

Scene

Hierarchy

TransformComponent

SpriteRendererComponent

CameraComponent
```

## Demo

通过：

```text
Scene
```

创建：

```text
Camera
Player
Enemy × N
```

Renderer 从 Scene 自动获取 Sprite。

## Benchmark

```text
10K Entity

100K Entity

Transform Iteration

Create / Destroy
```

## 核心知识

```text
Sparse Set

Cache Locality

Generational Index

Data-Oriented Design

Scene Graph

Transform Hierarchy
```

## 验收

Renderer 不再由 Sandbox 手动提交所有 Sprite 数据。

---

# 6. v0.4 — Asset + Serialization

状态：已完成

## 目标

让项目从“内存 Demo”进入“可持久化工程”。

## 范围

```text
UUID

Asset

AssetHandle

AssetMetadata

AssetRegistry

Texture Loader

Shader Loader

Asset Cache

AssetService

Scene Serialization

Scene Deserialization

SceneRenderer

Disk-backed Project Bootstrap
```

## Demo

用户：

```text
Create Scene
Save
Close
Reopen
```

Scene 完整恢复。

Sandbox 主路径进一步验证：

```text
Persistent Project Files
    ↓
AssetRegistry + Scene
    ↓
Runtime Reconstruction
    ↓
Asset Resolve / Cache
    ↓
Render
```

## Scene 中资源

从：

```text
C:/xx/player.png
```

迁移为：

```text
AssetHandle
```

## 核心知识

```text
Serialization

Schema

UUID

Handle

Cache

Resource Lifecycle

Atomic File Write
```

## 验收

- Scene 可可靠保存加载；
- 资源引用不依赖绝对路径；
- 同资源重复加载可命中 Cache；
- Sandbox 可从磁盘 AssetRegistry + Scene 恢复并渲染。

---

# 7. v0.5 — Lua Gameplay Runtime

状态：已完成

## 目标

实现 Engine 与 Gameplay 分离。

## 范围

```text
Lua VM

ScriptEngine

Lua Binding

LuaScriptComponent

ScriptInstance

OnCreate

OnUpdate

OnDestroy

Basic Hot Reload
```

## Demo

Lua：

```text
控制角色移动

修改 Transform

查询输入

访问 Entity
```

无需重新编译 C++ Engine。

## 核心知识

```text
Lua VM

C API

userdata

metatable

GC

Native / Script Lifetime

Hot Reload
```

## 验收

Gameplay 移动逻辑完全可以由 Lua 实现。

---

# 8. v0.6 — Editor Foundation

状态：已完成

## 目标

Janus 从 Runtime Framework 升级为真正 Game Engine。

## 范围

使用 Dear ImGui 实现：

```text
Editor Application

Hierarchy

Inspector

Scene View

Game View

Console

Asset Browser
```

## Scene View

至少：

```text
Pan

Zoom

Select Entity

Move Entity

Grid
```

## Play Mode

实现：

```text
EditorScene
 ↓
Clone
 ↓
RuntimeScene
 ↓
Stop
 ↓
Discard RuntimeScene
```

## 核心知识

```text
Editor Tooling

Framebuffer

Selection

Scene Picking

UI State

Runtime / Edit State Isolation
```

## 验收

不修改 C++ 代码即可：

```text
创建 Entity
添加基本 Component
设置 Sprite
保存 Scene
运行
```

---

# 9. v0.7 — Reflection + Command

状态：已完成

## 目标

建立 Human/Agent 共用的 Engine Capability Layer。

## 范围

### Reflection

```text
Type Metadata

Property Metadata

Component Registration

Inspector Auto Generation

Serialization Integration
```

### Command

```text
CommandBus

CreateEntityCommand

DeleteEntityCommand

AddComponentCommand

RemoveComponentCommand

SetPropertyCommand

Undo

Redo

Command History
```

## 改造

Editor 禁止继续直接：

```text
registry.emplace
transform.position = ...
```

所有修改进入 CommandBus。

## 核心知识

```text
Reflection

Metadata

Command Pattern

Undo / Redo

State Management
```

## 验收

用户通过 Editor：

```text
移动 Player
```

实际产生：

```text
SetPropertyCommand
```

并可：

```text
Undo
Redo
```

## 实现结果

v0.7 已形成第一条 Human/Agent 共用 Capability 主链：

```text
ProjectSession
├── ReflectionRegistry
├── CommandBus
└── EditorScene
      │
      ├── Reflection-backed Inspector
      ├── Reflection-backed Scene persistence
      └── reversible Scene commands
```

已落地：

- Core Reflection metadata 与稳定 ComponentTypeId / PropertyId；
- Transform、SpriteRenderer、Camera、LuaScript 内置 authoring metadata；
- Scene v1 序列化/反序列化迁移到 Reflection，SandboxProject 保持兼容；
- SetProperty / AddComponent / RemoveComponent Undo/Redo；
- Create / Delete / Rename Entity Undo/Redo；
- Delete Undo 恢复 persistent UUID、组件状态、Hierarchy 与 sibling order；
- Camera.primary 的跨实体 mutation delta 可完整撤销；
- EditorActions 全部作者态修改进入 ProjectSession CommandBus；
- Inspector 按 metadata / PropertyType 生成控件；
- Play Mode 禁止作者态 Execute / Undo / Redo，并保持 RuntimeScene 隔离；
- Windows CI 覆盖 Reflection、Command、Editor 与完整历史回归。

## 当前边界

v0.7 不提前实现：

```text
MCP Transport / JSON-RPC
Transaction / Command Grouping
Audit / Agent Activity
Profiler
Runtime Agent Control
User-facing Reparent UX
```

AssetReference 的通用 Inspector 在 v0.7 负责展示 UUID 与类型 metadata，类型安全赋值仍通过 Asset Browser。旧的多字段 EditorActions compatibility helper 允许产生多个 history entry；Transaction/批量原子操作留到 v0.9。

---

# 10. v0.8 — MCP Agent Foundation

状态：已完成

## 目标

Agent 成为 Janus 正式 Client。

## 范围

```text
MCP Protocol Core

JSON-RPC

stdio Transport

Tool Registry

Resource Registry

Schema Registry

Permission Foundation
```

## 首批 Resources

```text
engine://project/info

engine://scene/current

engine://scene/hierarchy

engine://entity/{id}

engine://asset/{id}
```

## 首批 Tools

```text
scene.create_entity

scene.delete_entity

scene.rename_entity

scene.add_component

scene.remove_component

scene.set_component_property

scene.save
```

## Reflection 联动

MCP Property Schema 来源于 Reflection。

## 核心知识

```text
Protocol Design

JSON-RPC

Schema

Adapter Pattern

Thread Boundary

Permission
```

## 验收

外部 Agent 可完成：

```text
读取 Scene
 ↓
创建 Player
 ↓
添加 SpriteRenderer
 ↓
设置 Transform
 ↓
保存
```

且用户可以在 Editor 中 Undo。

## 已完成验证

v0.8 已落地：

- native C++ JSON-RPC / stdio MCP stack；
- 2026-07-28 modern lifecycle 与 2025-11-25 legacy compatibility；
- deterministic ToolRegistry / ResourceRegistry / JSON Schema 2020-12；
- Reflection 驱动的 MCP property schema 与 authoring read model；
- Project / Scene / Hierarchy / Entity / Asset Resources；
- Create/Delete/Rename/Add/Remove/SetProperty/Save Tools；
- live JanusEditor MCP host、main-thread dispatcher 与 permission foundation；
- MCP mutation 与 Human authoring 共享 ProjectSession CommandBus；
- Play Mode write rejection、authoring read semantics、dirty/save semantics；
- protocol-only stdout 与 stderr diagnostics；
- GPU-independent real child-process stdio E2E，覆盖 modern/legacy transport 与 Scene persistence；
- Windows MSVC full regression：264/264 tests passed。

Hosted Windows CI 的真实 OpenGL driver 会在 shader compilation 上阻塞，因此 external protocol E2E 使用 test-only process fixture 隔离 GPU；live JanusEditor 的 shared Scene/CommandBus/Undo 行为由独立 Editor integration tests 覆盖。该测试分层不改变生产路径：真实用户/Agent 仍通过 `JanusEditor --project <path> --mcp-stdio` 使用 MCP。

---

# 11. v0.9 — Agent Development Loop

状态：已通过 PR #81 集成到 main（`273e35b`）；本地验收完成。

实施与构建/测试证据见 [v0.9 验证记录](verification/2026-09-06-v0.9-agent-development-loop.md)。尚未发布 Release。

- [v0.9 Agent Development Loop 设计方案](superpowers/specs/2026-09-06-v0.9-agent-development-loop-design.md)
- [v0.9 分阶段开发计划](superpowers/plans/2026-09-06-v0.9-agent-development-loop-plan.md)

## 目标

从“Agent 能修改”升级到“Agent 能调试和验证”。

## 范围

### Runtime

```text
runtime.play

runtime.pause

runtime.stop

runtime.step
```

### Runtime Resources

```text
runtime/status

runtime/entity/{id}
```

### Logs

```text
logs/recent
```

### Profiler

```text
CPU Scope Profiler

Renderer Statistics

Profiler Editor Panel

Profiler MCP Resource
```

### Transactions

```text
Transaction Begin

Commit

Rollback
```

### Audit

```text
Agent Activity
```

## 核心知识

```text
Instrumentation

RAII Profiling

Thread-Safe Trace

Transaction

Rollback

Audit
```

## 验收

Agent 可以：

```text
修改参数
 ↓
运行
 ↓
读取 Runtime
 ↓
读取日志
 ↓
判断结果
 ↓
停止
```

完成第一个 Agent Debug 闭环。

## 推荐实施顺序与阶段验收

| 阶段 | 交付内容 | 阶段验收 |
|---|---|---|
| A | Runtime 状态机、结构化日志、Runtime MCP | 修改 → 暂停启动 → 单步 → 读运行态/日志 → 停止；错误现场可观察 |
| B | CPU Profiler、两视图 Renderer Statistics、Editor/MCP | 同一份完成帧快照供 Human/Agent 使用，记录性能基线 |
| C | CommandGroup、Transaction、超时/断连处理 | Commit 一条历史、一次 Undo；失败完整回滚或明确隔离故障 |
| D | Audit/Agent Activity、完整 E2E 与文档 | Agent 操作和 Human Undo 可追踪，全部回归和真实 Editor 验证完成 |

Runtime Control 不进入作者态 Undo 历史；Paused/Faulted 仍禁止作者态写入。现有 Scene Resources 保持读取 EditorScene，新增 Runtime Resources 明确读取隔离运行世界。

v0.9 Transaction 仅覆盖当前 Scene 的可撤销作者态命令；不包含磁盘保存、资源文件修改或 Runtime 操作。采用独占写入、可见临时状态、逐命令校验及逆序回滚；完整失败语义见专项设计。

A/B/C 是中间交付门槛，不能代替 v0.9 的完整范围。D 收尾时必须包含 Profiler、Transaction 和 Audit，不因为最小 Debug 闭环已通就提前标记版本完成。

---

# 12. v0.10 — Game Systems

状态：**计划切片及综合示例已合入 main，发布准备中**。项目/输入、共享 Runtime、
UI/Text/Button、战斗/快照、动画、音频、物理、Prefab 和综合验收均在 `7ecdfb8` 中。
[综合验收](verification/2026-09-07-v0.10-integrated-acceptance.md)与
[集成复审](verification/2026-09-07-v0.10-integration-review.md)记录实现和测试；
[项目进度总表](project-status.md)列出各工作包的代码证据及剩余产品入口。

剩余发布工作：核对 Image/Animator 的 Human 资源赋值缺口，完成 Release 配置与目标设备
验证，更新 CMake/MCP 版本及发布说明。当前 CMake 为 0.9.0，无 Tag/Release；现有 Debug
CPU/FakeRenderDevice 基线不能替代 GPU/Present 或发布性能保证。本次状态更新不扩大或
缩减下方 v0.10 范围，也不将全部 PRD 欠账并入此版本。

## 目标

补齐真实 2D 游戏最基本能力。

## 范围

```text
Input Mapping

Sprite Animation

Animator

Basic UI

Audio

Physics 2D

Prefab Foundation

Project Settings
```

## UI

只要求：

```text
Canvas

Panel

Image

Text

Button
```

## Physics

Box2D 封装：

```text
RigidBody2D

Collider2D

Trigger

Raycast
```

## Animation

```text
AnimationClip

Animator

Loop

Play / Stop / Switch
```

## 验收

可以开发一个真正具备：

```text
菜单

角色

动画

碰撞

UI

声音
```

的小型游戏。

---

# 13. v0.11 — Production Demo

状态：尚未完成，待 v0.10 发布准备后立项。现有 Game/ 是固定单局与系统集成示例，不能代替本版本完整游戏。存档、多场景、内容规模、A–D Agent 成功率/失败模式基准，以及最小联网是否为发布硬门槛，需要在立项时明确；本次不删除下方联网目标。

## 目标

停止继续堆 Engine Feature。

开始使用 Janus 开发真实游戏。

## Demo

建议：

> 2D 卡牌 Roguelike 联机 Demo

## 重点验证

```text
Engine Public API

Lua Gameplay

UI

Asset Workflow

Scene Workflow

Profiler

MCP

Network
```

## Game 模块

示例：

```text
Card

Character

Battle

Buff

Enemy

Roguelike

Save Data

Network
```

必须全部存在：

```text
Game/
```

而不是 Engine。

## MCP Benchmark

设计固定 Agent 任务。

### Task A

```text
创建 Battle Test Scene
```

### Task B

```text
添加角色并配置 Sprite
```

### Task C

```text
定位 Health 不变化 Bug
```

### Task D

```text
分析 Draw Call 过高原因
```

记录 Agent 成功率与失败模式。

---

# 14. v1.0 — Janus Engine

状态：产品闭环尚未完成。项目创建、普通场景新建/切换、Duplicate、资产制作流程、正式 Agent 测试入口、生产 Headless 与权限/Dry Run 等需逐项决策并验收，见[PRD 对账](project-status.md)。已有测试 fixture、Command API 或示例内容不能自动代表产品入口已交付。

## 定义

v1.0 不意味着商业成熟。

而意味着：

> Janus 的核心产品假设已经完整成立。

必须满足以下闭环。

### Human

```text
Create Project
 ↓
Edit Scene
 ↓
Configure Assets
 ↓
Write Lua
 ↓
Play
 ↓
Debug
 ↓
Profile
 ↓
Build Demo
```

### Agent

```text
Connect
 ↓
Read Project
 ↓
Inspect Scene
 ↓
Modify
 ↓
Run
 ↓
Inspect Runtime
 ↓
Read Logs / Profiler
 ↓
Fix
 ↓
Validate
```

---

# 15. v1.0 必须具备

```text
C++20 Engine Core

2D Renderer

Batch Renderer

ECS

Scene

Hierarchy

Asset System

Serialization

Lua Runtime

Editor

Reflection

Command System

Undo / Redo

MCP

Runtime Control

Profiler

Transaction

Audit

Basic UI

Basic Animation

Basic Physics

Basic Audio

Real Demo Game
```

---

# 16. v1.0 明确不要求

```text
AAA 3D

Vulkan

DX12

Full PBR

Render Graph

Job System

Visual Scripting

Advanced Navigation

Advanced Particle Editor

Commercial Asset Store

Console Platform
```

---

# 17. v1.1+ 可选方向

v1.0 以后再根据求职和兴趣选择方向。

---

## 方向 A：Engine Systems

增加：

```text
Job System

Task Graph

Async Asset Loading

Memory Arena

Object Pool

Parallel ECS System
```

适合：

> 游戏引擎 / C++ Runtime 岗。

---

## 方向 B：Rendering

增加：

```text
3D Mesh

Perspective Camera

Material

Lighting

Shadow

PBR

Instancing

GPU Profiling
```

再进一步：

```text
Vulkan

Render Graph
```

适合：

> 图形 / 渲染 / 引擎岗。

---

## 方向 C：Agent Native

增加：

```text
Streamable HTTP MCP

Remote Agent

MCP Tasks

Build Tools

Scene Tests

Agent Test Runner

Asset Dependency Reasoning

Automatic Performance Diagnosis
```

适合强化：

> Janus 自身差异化。

---

## 方向 D：Networking

增加：

```text
Replication

Snapshot

Client State

Reconnect

Network Entity

Latency Simulation
```

配合 Game Server。

适合：

> 游戏客户端 + 服务端混合能力。

---

# 18. 开发优先级

整个项目必须始终保持：

```text
P0

Core Runtime
Renderer
ECS
Scene
Asset
Serialization
Lua
Editor
Command
MCP
```

其次：

```text
P1

Profiler
Animation
UI
Physics
Audio
Transaction
Test
```

最后：

```text
P2

3D
Vulkan
Job System
Render Graph
Advanced Agent
```

---

# 19. 每个版本的完成条件

版本不是按：

> “代码大概写完了。”

判断。

每个版本必须满足四项：

```text
Feature Complete

Demo Complete

Tests Pass

Documentation Updated
```

核心性能版本额外：

```text
Benchmark Recorded
```

---

# 20. 防止范围失控规则

任何新 Feature 加入 Roadmap 前必须回答：

1. 是否解决当前真实 Demo 的需求？
2. 是否是 Janus Agent-native 核心能力？
3. 是否能明显增加求职技术价值？
4. 是否会阻塞现有闭环？
5. 是否可以推迟到 v1.1？

若前三项均为否：

> 默认不进入 v1.0。

---

# 21. 推荐开发主线

实际研发时不要并行铺太多模块。

严格沿：

```text
Foundation
    ↓
Rendering
    ↓
ECS
    ↓
Scene
    ↓
Asset
    ↓
Lua
    ↓
Editor
    ↓
Reflection
    ↓
Command
    ↓
MCP
    ↓
Profiler
    ↓
Game Systems
    ↓
Real Game
```

展开。

---

# 22. 求职价值节点

## 第一阶段

完成：

```text
v0.1 ~ v0.3
```

已经可以讲：

```text
OpenGL

Renderer abstraction

Batch rendering

SparseSet ECS
```

---

## 第二阶段

完成：

```text
v0.4 ~ v0.7
```

可以讲：

```text
Asset lifecycle

Serialization

Lua Runtime

Editor

Reflection

Undo/Redo
```

此时项目已经具备较强简历价值。

---

## 第三阶段

完成：

```text
v0.8 ~ v0.9
```

可以形成 Janus 最大差异化：

```text
Native MCP

Agent Capability Layer

Transaction

Profiler

Agent Debug Loop
```

---

## 第四阶段

完成：

```text
v0.10 ~ v1.0
```

证明：

> Engine 不是技术玩具，而是真实可用工程。

---

# 23. 最终路线图总结

```text
                    Janus Roadmap

v0.1        Engine Foundation
               │
v0.2           Renderer
               │
v0.3         ECS / Scene
               │
v0.4       Asset / Storage
               │
v0.5         Lua Runtime
               │
v0.6           Editor
               │
v0.7     Reflection / Command
               │
v0.8            MCP
               │
v0.9     Agent Debug / Profiler
               │
v0.10       Game Systems
               │
v0.11        Real Game
               │
              v1.0
               │
      Human + Agent Engine Loop
```

Janus v1.0 的真正完成标志不是某一个技术模块。

而是下面这两句话同时成立：

> **一个开发者可以使用 Janus 独立完成一个真实 2D 游戏。**

以及：

> **一个 Agent 可以通过 Janus 的原生接口理解、修改、运行、调试和验证这个游戏。**

## 历史执行摘录（10-05a～10-06）

以下保持当时基线与下一包判断，相关代码现已合入 `7ecdfb8`；不用于判断当前待办或发布状态。

### 10-05a 本地执行更新

Button/焦点/OnClick/输入消费及可运行计数菜单已完成本地实现，见[验收记录](verification/2026-09-07-v0.10-ui-button.md)。本段覆盖历史表中的 Button 未实现状态；Text 已提交 PR #85，Button 以其为基线单独评审；尚未合并，下一包为 10-05b 固定战斗与结构化快照，不改变 v0.10 尚未整体完成的状态。


### 10-05b 执行状态更新（2026-09-07）

main 已更新至 `fbb1dc5`（#85），Text/布局已进入 main。基于 Button `973659d` 的 `codex/v0.10-playable-combat` 已本地实现菜单、选牌出牌、确定性胜负/重开及 Agent 结构化快照。Game/ 拥有规则，Engine 只提供有界标量诊断。见[设计](superpowers/specs/2026-09-07-v0.10-playable-combat-design.md)和[验收](verification/2026-09-07-v0.10-playable-combat.md)。本段覆盖旧的“下一包战斗/Text 未合并”状态，下一包为 10-06 AnimationClip/Animator，然后是 Audio、Physics、Prefab、综合验收。v0.10 尚未整体完成或发布。


### 10-06 执行状态更新（2026-09-07）

`codex/v0.10-animation` 保留未提交的 10-05b，并已本地实现 AnimationClip/Animator：有界 JSON 帧资产、Play/Stop/Switch、循环/单次结束、暂停/单步、Sprite/Image 运行时帧覆盖和 Lua 控制。战斗成功出牌播放边框反馈，独立 AnimationShowcase 展示世界 Sprite。见[设计](superpowers/specs/2026-09-07-v0.10-animation-design.md)与[验收](verification/2026-09-07-v0.10-animation.md)。本段覆盖上方“下一包 10-06”的历史状态；下一包为 **10-07 Audio**，之后 Physics、Prefab、综合验收。本包未提交、未合并，v0.10 未整体完成或发布。
