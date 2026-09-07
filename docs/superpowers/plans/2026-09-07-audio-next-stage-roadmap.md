# v0.10 最新进度与 10-07→10-10 路线

> 集成状态更新（2026-09-07，#92 合并后）：本文涉及的 v0.10 计划切片及综合示例均已进入 main `7ecdfb8`，合并后 CI 412/412 通过，尚未发布。以下分支、测试数量及“下一包/未提交/未合并”保留原记录时点；当前实现缺口与发布待办统一见[项目进度总表](../../project-status.md)。

日期：2026-09-07。沿用 PRD、技术架构及 A→F 分阶段路线，继续交付完整纵向切片。

## Git 调研

- 开始时工作区干净，位于 codex/v0.10-animation，25a3052。
- git fetch origin --prune 后，origin/main 仍为 fbb1dc5（#85）；本地 main 从
  273e35b 快进到该提交，包含项目/输入、共享 Runtime、UI 布局和 Text。
- Button #86 已进入 Text；Combat 92b8d13、Animation 25a3052 已提交；#88 将动画
  合入 origin/codex/v0.10-playable-combat，最新为 669c436。这些不等于已进入 main。
- codex/v0.10-audio 基于上述战斗/动画链，并合并最新 main；同步提交 be18e2e。
  保留已有分支和所有前期工作，不擅自合并远端 PR 或发布版本。

## 文档冲突处理

路线图、10-06 验收开头及旧阶段计划仍有“未提交/下一包动画”等历史描述，与 Git
事实不符。它们保留作为当时的验收基线；本记录及各文档顶部的当前状态覆盖旧状态。
PRD 的 P0 与路线图施工优先级 P1 不一致仍按既有决议解释：不删减 v0.10 必需能力。
架构要求“miniaudio 等成熟库”允许使用既有 SDL；具体锁定版本和边界见音频设计。

## 本轮范围与后续顺序

| 包 | 交付 | 完成门槛 |
| --- | --- | --- |
| 10-07 Audio（本轮） | PCM WAV 资产、AudioSource、Lua 控制、共享 Runtime 混音、暂停/停止/故障、出牌与背景声 | 反射/保存/Clone/Undo/MCP、fake 设备、双宿主、外部快照、本机设备 smoke、两套构建及全 CTest |
| 10-08 Physics（下一包） | Box2D 私有封装、RigidBody2D/Collider2D/Trigger/Raycast、碰撞事件及固定 tick | 先设计可变渲染步与固定物理步的关系；保持中性 Step=1/60；独立物理验证场景；双宿主一致 |
| 10-09 Prefab | 模板资产、实例化与实际使用的 UUID/实体引用重映射 | active Reflection、共享命令、一次 Undo、Scene round-trip；不宣称未实现的引用类型已重映射 |
| 10-10 综合验收 | 八项 v0.10 能力对账、集成 Demo、Agent 固定任务与性能基线 | 功能、Demo、自动回归、真实设备和文档同时闭环后决定发布 |

Physics 下一包开始前必须说明 Box2D 精确版本、PUBLIC/PRIVATE 边界、所有权、固定步
积累/追赶上限、接触事件派发顺序、暂停/故障/销毁策略以及 Scene/Transform 同步。
这些是后续设计输入，本轮不提前实现物理或更改既有脚本 Update 时序。

音频实现和验证分别见 [设计](../specs/2026-09-07-v0.10-audio-design.md)、
[验收](../../verification/2026-09-07-v0.10-audio.md)。v0.10 尚未整体完成或发布。
