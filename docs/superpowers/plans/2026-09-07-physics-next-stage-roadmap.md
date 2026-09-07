# v0.10 10-08 完成后路线

日期：2026-09-07。开始时工作区干净，Audio 已提交为 b4eaab7。
本轮 `git fetch origin --prune` 后，main / origin/main 仍为 fbb1dc5，
不需要再次合并。新建 codex/v0.10-physics 接续音频依赖链。
战斗、动画、音频与物理仍未全部进入 main，不能据本地功能完成宣称版本发布。

## 已完成的物理切片

Box2D 3.1.1 PRIVATE 封装、RigidBody2D/Collider2D、固定 60 Hz、八次追赶上限、
Transform 同步、阻挡、触发器、射线、Lua 事件/冲量/速度/延迟销毁、双宿主和
现有 MCP 快照观察。仅根节点单位缩放的矩形刚体；复杂形状、关节与物理编辑器
不进入本包。依据：[设计](../specs/2026-09-07-v0.10-physics-design.md)、
[验收](../../verification/2026-09-07-v0.10-physics.md)。

## 下一包 10-09 Prefab

先做一个能保存、实例化、重新打开并 Undo/Redo 的实体子树切片：

1. 明确 Prefab 文件 schema/version、资产类型、子树根和外部引用边界；持久 UUID
   与实例 UUID 必须分开，每次实例化重映射内部实体引用，检测断链/循环/过深内容。
2. 序列化/克隆使用 active ReflectionRegistry，复用 Scene v1 字段兼容约定。
   先核对现有 EntityReference 属性，不能只复制 UUID 文本；新游戏组件也应保真。
3. 创建实例通过共享 Engine Scene 命令与 ProjectSession guards。Human/MCP
   使用同一条路径；事务、Runtime、恢复锁和脏标记语义保持一致。
4. 初版明确为展开实例，或先确定可实现的来源引用/覆盖策略；不能悄然承诺完整的
   apply/revert、嵌套 Prefab 和实时传播。设计记录应解释首版与后续增量的边界。
5. 提供带 Sprite、脚本及一个游戏系统组件的示例子树。自动验证重复实例化身份
   唯一、内部引用重映射、保存重载、失败原子性、Undo/Redo、Runtime 隔离及 MCP。

## 随后 10-10 综合验收

按 PRD/技术架构/版本路线图逐项核对 v0.10 八项能力。将项目配置、Input Actions、
共享执行、UI/Text/Button、动画、音频、物理和 Prefab 连接为可玩 Demo，补 Agent
固定验收任务、CPU 性能基线与真实设备记录。CPU 分阶段 profiler 接入和综合性能
预算仍须在该阶段明确；本包不把总 Runtime 耗时称为物理或 GPU 耗时。

整理依赖分支的集成顺序与逐包验收证据后再进入 main 审查；发布需单独对照全部
门槛。当前下一包是 Prefab，v0.10 尚未整体完成或发布。
