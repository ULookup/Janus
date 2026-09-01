# Janus Engine 产品需求文档（PRD）

**文档版本：** v0.1  
**产品名称：** Janus Engine  
**产品类型：** Agent-Native C++ Game Engine  
**首期产品形态：** Windows 桌面端 2D 游戏引擎 + Editor + MCP Agent Interface  
**核心技术方向：** C++20 / OpenGL / Lua / MCP  
**文档状态：** 产品立项版

---

# 1. 产品概述

## 1.1 产品定义

Janus Engine 是一个面向 2D 游戏开发的轻量级 C++ 游戏引擎。

与传统游戏引擎不同，Janus Engine 从产品设计阶段同时考虑两类一等用户：

- Human Developer；
- AI Agent。

Human Developer 通过图形化 Editor 操作项目。

AI Agent 通过 MCP 获取项目上下文，并以结构化方式读取、修改、运行、调试和验证游戏项目。

核心产品模型：

```text
                   Janus Engine
                        │
             ┌──────────┴──────────┐
             │                     │
             ▼                     ▼
      Human Interface        Agent Interface
             │                     │
          Editor                  MCP
             │                     │
             └──────────┬──────────┘
                        ▼
                 Engine Capability
                        │
              ┌─────────┼─────────┐
              ▼         ▼         ▼
            Scene      Asset    Runtime
              │         │         │
             ECS      Resource  Profiler
```

Janus 的核心产品理念为：

> **A game engine built for humans and agents.**

---

# 2. 产品背景

当前游戏开发工具链主要围绕人工操作设计。

典型工作流：

```text
Developer
    ↓
Editor
    ↓
Modify Scene / Script
    ↓
Run Game
    ↓
Observe
    ↓
Debug
    ↓
Modify Again
```

AI Agent 已逐渐能够完成：

- 代码编写；
- 项目分析；
- 自动测试；
- Bug 修复；
- 工程重构。

但在游戏开发领域仍存在明显断层。

AI 通常只能：

```text
读取代码
修改文件
运行命令
```

却无法可靠理解：

- 当前场景中有哪些 Entity；
- Entity 具有什么 Component；
- 当前游戏是否正在运行；
- 某资源被哪些对象引用；
- 当前一帧的性能瓶颈；
- 当前运行中的实际 Component 状态；
- Editor 中当前打开了什么 Scene。

因此 AI 对游戏项目的认知往往是不完整的。

Janus Engine 希望让 Engine 本身成为 Agent 可以理解和操作的结构化环境。

---

# 3. 产品愿景

Janus Engine 希望验证一种新的游戏开发范式：

```text
            Developer
              │
              │ Natural Language
              ▼
            Agent
              │
              │ MCP
              ▼
          Janus Engine
              │
     ┌────────┼────────┐
     ▼        ▼        ▼
   Inspect   Modify    Run
     │        │        │
     └────────┼────────┘
              ▼
           Validate
```

最终让 AI Agent 可以完成：

```text
理解项目
→ 制定修改方案
→ 修改场景
→ 修改参数
→ 运行游戏
→ 查看日志
→ 查看运行状态
→ 查看性能
→ 发现问题
→ 继续修改
```

形成完整开发闭环。

Janus 并不试图使用 AI 取代游戏开发者。

产品目标是：

> 将 AI Agent 变成能够真正使用游戏引擎工具链的工程协作者。

---

# 4. 产品目标

## 4.1 P0：完整的 2D Engine Runtime

Janus 必须首先是一个可以实际开发游戏的 Engine。

必须具备：

- Game Loop；
- Scene；
- Entity；
- Component；
- 2D Rendering；
- Camera；
- Input；
- Animation；
- Asset；
- Lua；
- UI；
- Audio；
- Physics Integration；
- Serialization。

---

## 4.2 P0：可视化 Editor

用户无需通过修改 C++ 源代码完成所有游戏内容。

Editor 至少提供：

```text
Hierarchy
Inspector
Scene View
Game View
Asset Browser
Console
Profiler
```

---

## 4.3 P0：Agent Native

Agent 必须能够通过正式 Engine Interface 获取：

```text
Project
Scene
Entity
Component
Asset
Runtime
Log
Profiler
```

而不是通过：

- OCR；
- 模拟鼠标；
- 直接修改内部内存；
- 猜测项目状态。

---

## 4.4 P1：Human / Agent 同源操作

所有修改行为最终进入统一 Engine Command Layer。

目标模型：

```text
Editor ─────┐
            │
MCP ────────┼──► Engine Command Layer
            │
CLI ────────┘
```

禁止长期形成：

```text
Editor Implementation

以及另一套

MCP Implementation
```

---

## 4.5 P1：可观察性

Engine 必须具备足够的内部可观察能力。

包括：

```text
Logs
Runtime State
Renderer Statistics
Profiler
Asset References
Entity State
Errors
```

可观察性同时服务：

- Developer；
- Debugger；
- Agent。

---

# 5. 非产品目标

Janus v1 阶段不追求成为通用 AAA Engine。

以下需求明确不属于首期核心目标：

```text
AAA 3D Renderer
大型开放世界
Terrain
Nanite 类技术
实时全局光照
Visual Scripting
Shader Graph
完整粒子编辑器
完整 Navigation System
自研 Physics Solver
主机平台支持
移动平台全面适配
Asset Store
完整 Multiplayer Engine
完整动画骨骼编辑器
```

对于暂未支持的能力：

> 优先保持架构扩展性，而不是提前开发。

---

# 6. 目标用户

## 6.1 C++ 独立开发者

特征：

- 熟悉 C++；
- 希望更直接控制 Runtime；
- 不需要 Unity / Unreal 全部复杂能力；
- 关注引擎内部工作方式。

核心需求：

```text
轻量
透明
可调试
可扩展
```

---

## 6.2 游戏开发学习者

希望通过 Janus 理解：

- ECS；
- Scene；
- Rendering；
- Asset；
- Lua Runtime；
- Editor；
- Profiling。

Janus 应保持核心模块足够清晰，避免过度魔法化。

---

## 6.3 Agent-assisted Developer

开发者长期使用：

- Codex；
- Claude；
- ChatGPT；
- IDE Agent；

完成游戏工程开发。

核心需求：

> Agent 不仅能修改代码，还能真正理解和操作游戏项目。

这是 Janus 最具有差异化的目标用户。

---

# 7. 核心使用场景

## 7.1 创建游戏项目

用户启动 Janus Editor。

选择：

```text
Create Project
```

填写：

```text
Project Name

Location
```

创建项目后默认生成：

```text
Project/
├── Assets/
├── Scripts/
├── Scenes/
├── Config/
└── project.json
```

系统创建默认 Scene。

---

# 8. 编辑场景

用户可以在 Hierarchy 中：

```text
Create Entity
Delete Entity
Rename Entity
Duplicate Entity
Reparent Entity
```

选择 Entity 后 Inspector 展示其 Components。

例如：

```text
Player

Transform

Position
Rotation
Scale

SpriteRenderer

Texture
Layer

LuaScript

Script
```

---

# 9. 添加 Component

用户点击：

```text
Add Component
```

搜索：

```text
SpriteRenderer
```

完成添加。

组件类型来自 Engine Component Registry。

Editor 不允许写死所有 Component 类型。

未来自定义 Component 也应能够注册至 Editor。

---

# 10. Scene View

Scene View 用于编辑游戏世界。

必须支持：

- Camera Pan；
- Camera Zoom；
- Entity Selection；
- Entity Highlight；
- Move Gizmo；
- Grid；
- Sprite Rendering。

后续支持：

- Rotate Gizmo；
- Scale Gizmo；
- Snapping。

---

# 11. Game View

Game View 展示实际 Runtime Camera 输出。

必须与 Scene View 区分。

```text
Scene View

编辑视角

≠

Game View

游戏摄像机视角
```

进入 Play 后 Game View 自动显示运行内容。

---

# 12. Play Mode

Editor 提供：

```text
Play
Pause
Stop
```

进入 Play Mode：

```text
Editor Scene
      ↓
Runtime Scene Instance
      ↓
Game Loop
```

停止运行后：

> 默认不将 Runtime 产生的修改写回编辑状态。

防止游戏逻辑污染原始场景。

---

# 13. Scene 保存与加载

用户保存 Scene：

```text
Battle.scene
```

Scene 至少保存：

- Entity UUID；
- Entity Name；
- Hierarchy；
- Components；
- Component Properties；
- Asset References。

重新加载后应恢复一致状态。

---

# 14. Asset Browser

Asset Browser 展示项目 Assets。

首期资源类型：

```text
Texture
Scene
Lua Script
Shader
Audio
Animation
```

支持：

- 搜索；
- 类型筛选；
- 文件夹浏览；
- Import；
- Reimport；
- Rename；
- Delete。

---

# 15. Asset Inspector

选中资源后展示：

```text
Asset Name
Asset Type
Asset Handle
Path
Import Settings
Dependencies
References
```

例如 Texture：

```text
Filtering
Wrap Mode
```

---

# 16. Lua Gameplay

游戏 Gameplay 可以由 Lua 编写。

Entity 添加：

```text
LuaScriptComponent
```

指定：

```text
player.lua
```

基本生命周期：

```text
OnCreate

OnUpdate

OnDestroy
```

Agent 和 Human 均可查看当前 Script。

---

# 17. Lua Hot Reload

开发者修改 Lua Script 后可以：

```text
Reload Script
```

无需重新编译 Engine。

如果游戏正在运行，应尽可能重新加载对应 Script Instance。

首期允许某些 Runtime State 丢失。

后续再考虑完整状态迁移。

---

# 18. Animation

首期主要解决 Sprite Animation。

用户可以创建：

```text
Animation Clip
```

定义：

```text
Frames
Frame Duration
Loop
```

Animator 支持：

```text
Play
Stop
Switch Clip
```

后续增加 Animation State Machine。

---

# 19. Game UI

首期 UI 服务真实 Demo 游戏需求。

必须支持：

```text
Canvas
Panel
Image
Text
Button
```

需要：

- 层级；
- Anchor；
- Pivot；
- Basic Layout；
- Click Event。

不追求完整 Unity UGUI 功能。

---

# 20. Console

Console 必须统一展示：

```text
Engine
Renderer
Asset
Lua
Gameplay
MCP
```

日志。

每条日志包含：

```text
Time
Level
Category
Message
```

支持：

```text
Info
Warning
Error
```

筛选。

---

# 21. Profiler

Profiler 是 Janus 首期重要能力。

至少采集：

```text
Frame Time
Update Time
Render Time
Lua Time
Physics Time
Animation Time

Entity Count

Draw Call
Batch Count
Sprite Count
```

Profiler 数据必须同时可被：

- Editor；
- MCP；

读取。

---

# 22. Renderer Statistics

单独提供 Renderer Debug 信息。

例如：

```text
Sprites

Draw Calls

Batch Count

Texture Bind Count

Vertex Count

Index Count
```

主要服务性能定位。

---

# 23. MCP 产品能力

MCP 是 Janus Agent Interface 的第一种标准协议实现。

MCP 功能分为：

```text
Read
Modify
Runtime
Diagnostics
Automation
```

---

# 24. Agent Project Context

Agent 可以获取：

```text
Project Name
Project Path
Engine Version
Current Scene
Play Status
Asset Count
Entity Count
```

典型需求：

用户：

> 看一下当前项目是什么状态。

Agent无需读取大量项目文件即可获得结构化 Project Context。

---

# 25. Agent Scene Query

Agent 可以查询当前 Scene。

包括：

```text
Scene Information

Hierarchy

Entity List

Entity Components

Component Properties
```

Agent 应支持按：

```text
ID
Name
Component Type
```

寻找 Entity。

---

# 26. Agent Scene Modification

Agent 可以：

```text
Create Entity
Delete Entity
Rename Entity
Duplicate Entity
Reparent Entity

Add Component
Remove Component

Set Component Property
```

Agent 行为必须产生与 Editor 相同的 Command。

---

# 27. Agent Asset Query

Agent 可以搜索：

```text
Texture
Script
Scene
Audio
Animation
```

例如：

用户：

> 给 Player 找一张角色 Sprite。

Agent：

```text
asset.search
```

找到候选资源。

随后可以设置 Player 的 SpriteRenderer。

---

# 28. Agent Asset Operation

允许 Agent：

```text
Import
Reimport
Rename
Move
```

首期对：

```text
Delete
Overwrite
Batch Modification
```

采用更严格权限。

---

# 29. Agent Runtime Control

Agent 可以：

```text
Play
Pause
Stop
Step Frame
```

用于自动调试和验证。

例如：

```text
Modify
↓
Play
↓
Inspect
↓
Stop
```

形成闭环。

---

# 30. Runtime State Query

游戏运行过程中 Agent 可以读取：

```text
Entity Runtime State
Component Runtime State
Current Scene
Game Time
Pause State
```

例如：

用户：

> 为什么 Enemy 没掉血？

Agent 可以直接读取 Enemy：

```text
Health.current
```

而不是只能分析代码推测。

---

# 31. Agent Log Access

Agent 可以获取：

```text
Recent Error

Recent Warning

Lua Error

Renderer Error

Gameplay Log
```

支持：

```text
category
level
time range
```

筛选。

---

# 32. Agent Profiler Access

Agent 可以请求：

```text
Latest Frame

N Frame Capture

Renderer Statistics
```

Agent应可以回答：

> 当前性能瓶颈主要在哪里？

后续 Agent 可以进一步提出优化方案。

---

# 33. Agent Testing

Janus 需要提供自动测试入口。

至少：

```text
List Tests
Run Test
Run Test Suite
Get Result
```

后续支持：

```text
Run Scene Test
```

典型 Agent 闭环：

```text
发现 Bug
 ↓
修改
 ↓
运行 Test
 ↓
失败
 ↓
读取日志
 ↓
再次修改
 ↓
通过
```

---

# 34. Command System 产品要求

Editor 和 Agent 的修改必须统一映射到 Command。

核心 Command 类型：

```text
CreateEntity

DeleteEntity

RenameEntity

ReparentEntity

AddComponent

RemoveComponent

SetProperty
```

Command 需要能够反馈：

```text
Success

Failure

Validation Error

Affected Object
```

---

# 35. Undo / Redo

Human 在 Editor 中的操作支持：

```text
Undo

Redo
```

Agent 产生的修改同样进入历史。

例如：

```text
Agent 删除 Enemy
```

用户随后点击：

```text
Undo
```

必须可以恢复。

---

# 36. Agent 操作历史

Editor 提供 Agent Activity 面板。

展示：

```text
12:31:02

Agent

Created Entity:
Enemy_01
```

以及：

```text
12:31:04

Agent

Set:
Enemy_01.Transform.position

Old:
[0,0]

New:
[100,20]
```

让 Human 清楚知道 Agent 修改了什么。

---

# 37. Transaction

Agent 可以将多个修改作为一个 Transaction。

例如：

```text
Create Player

+

Add Transform

+

Add SpriteRenderer

+

Set Texture

+

Add LuaScript
```

应视为一个逻辑操作。

如果中间发生错误：

```text
Rollback
```

避免生成残缺对象。

---

# 38. Dry Run

高风险操作支持：

```text
Dry Run
```

例如：

> 删除所有未引用资源。

Janus 应先返回：

```text
Would Delete

24 Textures

3 Audio

2 Scripts
```

不立即执行。

---

# 39. Agent Permission

Agent 权限分级：

| Level | 能力 |
|---|---|
| ReadOnly | 查询 Project / Scene / Asset / Log / Profiler |
| Editor | 修改 Scene 和常规 Asset |
| Developer | Runtime / Test / Build / Script Reload |
| Elevated | 删除资源等高风险行为 |

默认 Agent 不获得最高权限。

---

# 40. Headless

Janus 必须支持无 Editor UI 模式。

目标：

```text
Janus --headless
```

支持：

```text
Load Project
Load Scene
Run Test
Run Simulation
Asset Import
MCP
```

为以下场景服务：

```text
CI

Remote Agent

Automation
```

---

# 41. Project Settings

项目提供 Settings。

首期：

```text
Project Name

Default Scene

Resolution

VSync

Target FPS

Asset Root

Script Root
```

后续扩展：

```text
Input Mapping

Physics Settings

Render Settings
```

---

# 42. Input Mapping

避免 Gameplay 直接绑定具体按键。

用户定义：

```text
MoveUp = W

MoveDown = S

Confirm = Space
```

Gameplay 查询：

```text
Action
```

而不是：

```text
Keyboard W
```

---

# 43. Error Handling

核心原则：

> Engine Error 不应直接导致 Editor 崩溃，除非进入不可恢复状态。

例如：

```text
Texture Load Failed
```

应：

```text
显示 Error Asset

记录 Console

继续运行 Editor
```

Lua Script Error：

```text
停止对应 Script

记录 Stack Trace

Game Runtime 尽量继续
```

---

# 44. 用户体验原则

Janus Editor 不追求复杂视觉效果。

第一阶段核心设计目标：

```text
Fast

Predictable

Readable

Developer-Oriented
```

不追求：

```text
大量动画

复杂视觉特效

高度定制界面
```

---

# 45. 项目生命周期

用户打开：

```text
Janus Editor
```

工作流程应为：

```text
Project Selection
       ↓
Open Project
       ↓
Load Editor
       ↓
Open Default Scene
       ↓
Edit
       ↓
Save
       ↓
Play
       ↓
Debug
       ↓
Stop
```

Agent 在 Editor 打开后自动获得当前 Project Context。

---

# 46. MVP 范围

Janus MVP 只要求打通最重要闭环。

## Human MVP

必须完成：

```text
Create Project
 ↓
Open Project
 ↓
Create Scene
 ↓
Create Entity
 ↓
Add Sprite
 ↓
Move Entity
 ↓
Save Scene
 ↓
Play
 ↓
Lua Update
 ↓
Stop
```

## Agent MVP

必须完成：

```text
Connect
 ↓
Read Project
 ↓
Read Scene
 ↓
Create Entity
 ↓
Add Component
 ↓
Set Property
 ↓
Save
 ↓
Play
 ↓
Read Runtime State
 ↓
Stop
```

两条链同时完成：

> Janus MVP 成立。

---

# 47. MVP 明确不要求

MVP 不要求：

```text
完整 UI System

完整 Physics

Animation State Machine

3D

HTTP MCP

完整 Profiler

Asset Dependency Graph

复杂 Permission

Build Pipeline

完整 Headless

Network Gameplay
```

防止 MVP 无限扩张。

---

# 48. V0.1

主题：

> Engine Foundation

目标：

```text
Application / MainLoop

SDL3 Window

OpenGL Context

Logging / Assertion

Timer

Basic Event / Keyboard Input

FileSystem Utility

Unit Test Framework
```

验收：

> Sandbox 通过 Engine Application API 稳定运行空窗口，并支持关闭、缩放和键盘输入。

---

# 49. V0.2

主题：

> Renderer2D

增加：

```text
RenderDevice / OpenGLRenderDevice

Buffer / Shader / Texture / Framebuffer

Orthographic Camera

Sprite Renderer / Render Queue / Batch Renderer

Renderer Statistics
```

验收：

> Sandbox 可以渲染多纹理、透明 Sprite，并记录 1K/10K Sprite Benchmark；客户端不暴露 OpenGL 类型或调用。

---

# 50. V0.3

主题：

> ECS + Scene

增加：

```text
Entity / SparseSet / ComponentPool / Registry / View

Scene / Hierarchy

TransformComponent

SpriteRendererComponent

CameraComponent
```

验收：

> Renderer 从 Scene 自动获取 Sprite，Sandbox 不再手动提交全部渲染数据。

---

# 51. V0.4

主题：

> Asset + Serialization

增加：

```text
UUID / AssetHandle / AssetMetadata / AssetRegistry

Texture Loader / Shader Loader / Asset Cache

Scene Serialization / Deserialization
```

验收：

> Scene 可以可靠保存并重新打开，资源引用不依赖绝对路径。

---

# 52. V0.5

主题：

> Lua Gameplay Runtime

增加：

```text
Lua VM / ScriptEngine / Lua Binding

LuaScriptComponent / ScriptInstance

OnCreate / OnUpdate / OnDestroy

Basic Hot Reload
```

验收：

> Gameplay 移动逻辑可以完全由 Lua 实现，无需重新编译 C++ Engine。

---

# 53. V0.6

主题：

> Editor Foundation

增加：

```text
Editor Application

Hierarchy / Inspector

Scene View / Game View

Console / Asset Browser

Play Mode Scene Isolation
```

验收：

> 不修改 C++ 代码即可创建 Entity、添加基础 Component、设置 Sprite、保存并运行 Scene。

---

# 54. V0.7

主题：

> Reflection + Command

增加：

```text
Type / Property / Component Metadata

Inspector / Serialization Reflection Integration

CommandBus / Command History

Create / Delete / Add / Remove / SetProperty Command

Undo / Redo
```

验收：

> Editor 修改统一进入 CommandBus，属性修改可以 Undo / Redo。

后续版本边界统一为：

```text
v0.8  MCP Agent Foundation
v0.9  Agent Development Loop
v0.10 Game Systems
v0.11 Production Demo
v1.0  Core Product Loop Complete
```

本节只提供产品级摘要。所有版本的详细范围、Benchmark 和完成条件均以《Janus Engine 版本路线图》为唯一来源；PRD 不再维护另一套独立版本边界。

---

# 55. Demo Game

Janus 不采用纯技术 Sandbox 作为最终验证。

需要开发一款具有：

```text
Main Menu

Gameplay

UI

Animation

Save Data

Audio

Multiple Scenes
```

的完整 2D 游戏 Demo。

首选项目：

> 2D 卡牌 Roguelike Demo。

原因：

- 强 UI；
- 强 Lua Gameplay；
- 强 Asset；
- 适合数据驱动；
- 对 Physics 需求较低；
- 可以验证 Networking；
- 适合 Agent 自动测试。

---

# 56. 产品质量指标

Janus 不是商业成熟产品，因此不设传统 DAU 等指标。

核心质量指标为：

### 稳定性

典型 Editor 操作不得高频 Crash。

### 可恢复性

错误 Asset、错误 Lua 等问题不得导致整个项目不可打开。

### 可观察性

关键 Engine State 必须可诊断。

### 可验证性

核心模块拥有自动测试或 Benchmark。

### Agent 可操作性

核心 Editor 行为存在 Agent 等价操作。

---

# 57. Agent 成功率指标

对于固定测试任务：

> 创建一个指定场景。

Agent 在无人工 Editor 操作情况下必须能够：

```text
创建 Scene

创建 Camera

创建 Player

设置 Sprite

添加 Script

保存
```

最终 Scene 与预期结构一致。

---

# 58. Agent Debug Benchmark

设计一个固定 Bug：

```text
Player 攻击 Enemy

Enemy Health 不变化
```

Agent需要能够：

```text
Inspect Runtime

Read Logs

Inspect Components

定位问题

修改

重新测试
```

以此作为 Agent-native 能力的长期 Benchmark。

---

# 59. Renderer Benchmark

建立固定 Benchmark Scene：

```text
1K Sprite

10K Sprite

50K Sprite
```

记录：

```text
FPS

Frame Time

Draw Calls

Batch Count
```

优化必须提供前后结果。

---

# 60. ECS Benchmark

固定测试：

```text
10K Entity

100K Entity

Transform Iteration

Add Component

Remove Component

Query
```

用于发现设计退化。

Benchmark 不作为追求极端性能的 KPI。

主要用于：

> 防止引擎演进过程中性能无意识下降。

---

# 61. Compatibility

首期只承诺：

```text
Windows x64
```

开发过程中保持：

```text
Platform Abstraction
```

后续评估：

```text
Linux

macOS
```

不在早期同时维护多个平台。

---

# 62. 安全需求

MCP 暴露后产生新的安全边界。

Agent 默认不得：

```text
访问 Project Root 外文件

执行任意 Shell

访问系统凭据

修改 Engine Binary

加载任意 Native Library
```

所有文件操作限制在：

```text
Project Scope
```

内。

---

# 63. 数据安全

所有 Agent 修改应尽可能：

```text
可 Undo

可 Audit

可 Transaction Rollback
```

Scene 保存前后采用安全写入策略：

```text
Write Temp

Validate

Replace Original
```

降低工程损坏风险。

---

# 64. 产品风险

## 范围膨胀

这是项目最大风险。

必须始终坚持：

> 做完整闭环，而不是做功能数量。

例如：

宁可：

```text
完整 2D Renderer
```

也不要：

```text
半成品 2D

+

半成品 3D
```

---

## MCP 掩盖引擎核心

不能因为 Agent 是产品特色，就让大量研发时间提前投入 MCP。

正确顺序：

```text
Engine Capability

↓

Agent Interface
```

没有稳定 Engine Capability：

> MCP 没有价值。

---

## Editor 工程量失控

Editor 是巨大系统。

首期只支持真正需要的：

```text
Hierarchy

Inspector

Scene

Game

Assets

Console

Profiler
```

避免开发：

```text
完整 Dock System

复杂 Theme Editor

Animation Graph Editor

Visual Scripting
```

---

# 65. 核心产品差异化

Janus 不以：

```text
比 Unity 功能更多

比 Unreal 画质更好

比 Godot 更成熟
```

作为竞争方向。

Janus 的差异化是：

> **Agent 是一等 Engine Client。**

传统模式：

```text
AI
 ↓
文件系统
 ↓
游戏工程
```

Janus：

```text
AI
 ↓
Engine Protocol
 ↓
Engine State
```

区别在于：

AI 不只是编辑项目文件。

AI 可以：

```text
Observe

Understand

Modify

Execute

Measure

Validate
```

---

# 66. 产品原则总结

Janus 后续所有需求决策遵循以下原则：

```text
1. Engine 首先必须能真正做游戏。

2. Engine Capability 高于协议能力。

3. Human 与 Agent 是平等客户端。

4. Editor 与 MCP 不重复业务逻辑。

5. 数据优先于对象魔法。

6. 核心状态必须可观察。

7. Agent 行为必须可验证。

8. 破坏性修改必须可控制。

9. 每个抽象都需要真实需求。

10. 性能优化必须来自测量。

11. 先完成 2D，再讨论 3D。

12. 先完成闭环，再扩展功能。
```

---

# 67. 最终产品体验

理想情况下，一个开发者可以打开 Janus，对 Agent 说：

> 创建一个战斗测试场景。使用现有 Knight 角色资源，在中心生成 Player，在右侧生成三个 Enemy，为 Player 添加移动脚本，然后运行场景确认角色可以正常移动。

Agent：

```text
读取 Project
 ↓
搜索 Knight Asset
 ↓
创建 Scene
 ↓
创建 Camera
 ↓
创建 Player
 ↓
添加 SpriteRenderer
 ↓
添加 Collider
 ↓
绑定 Lua Script
 ↓
创建 Enemy × 3
 ↓
保存 Scene
 ↓
Play
 ↓
Inspect Runtime
 ↓
确认运行结果
```

如果发生错误：

```text
读取 Console
 ↓
定位 Lua Error
 ↓
修复
 ↓
Reload
 ↓
重新运行
```

开发者可以在 Editor 中实时查看 Agent 的每一次操作，并：

```text
接受

修改

Undo
```

这就是 Janus Engine 最终希望验证的游戏开发工作流：

> **Human 与 Agent 共享同一个 Engine、同一个 Project State、同一套 Command、同一个反馈闭环。**

Janus 的核心价值不是“游戏引擎里加入 AI”。

而是：

> **重新定义一个游戏引擎应该如何向 Human 和 Agent 同时暴露自己的能力。**
