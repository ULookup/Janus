# Janus Engine 版本路线图

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

状态：下一里程碑

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

Scene Serialization

Scene Deserialization

Asset Cache
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
- 同资源重复加载可命中 Cache。

---

# 7. v0.5 — Lua Gameplay Runtime

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

---

# 10. v0.8 — MCP Agent Foundation

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

---

# 11. v0.9 — Agent Development Loop

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

---

# 12. v0.10 — Game Systems

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
