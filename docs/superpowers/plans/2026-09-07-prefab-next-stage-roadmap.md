# v0.10 main 同步、Prefab 与下一阶段路线

日期：2026-09-07。开始时在 main，工作区干净。执行 `git fetch origin` 和
`git merge --ff-only origin/main`，本地/远端一致为 `b9b990b`，输出 Already up to date。
随后创建 `codex/v0.10-prefab`，保留 main 作为集成基线。

## 调研结论

| 能力 | Git/代码证据 | 本轮处理 |
|---|---|---|
| Project Settings / Action Input / Shared Runtime | main 含 Stage A、#83 与 RuntimeExecution | 复用 |
| UI/Text/Button / Combat / Animation | #91 的依赖链已进入 main，Game 与 UI/Animation 目录存在 | Prefab 保真与运行验证 |
| Audio | #89 / b4eaab7 已在 main | 序列化保真回归 |
| Physics | #90 / 361ee3e 已在 main | 序列化保真回归 |
| Prefab Foundation | 基线没有模块，最新物理后续计划明确下一包 10-09 | 本轮实现 |
| 综合游戏、设备、CPU 性能验收 | 之前各专项验收不能替代综合验收 | 10-10 |

AGENTS.md 和路线图中的 fbb1dc5、依赖分支未合并是过时执行状态，已更新当前状态。
PRD 要求 Human/Agent 等价制作能力，路线图要求 Prefab Foundation，技术架构把
Prefab Override 留作未来扩展；据此选择展开子树模板，无需扩大到持续关联实例。
没有引入新依赖，也没有将完整 Roguelike/联网提前至 v0.10。

## 10-09 交付

有界 versioned Prefab、共享 Reflection/Scene v1、子树 UUID/parent 重映射、
注册资产导出与加载、共享命令 Undo/Redo/事务、Human 面板和 MCP、双机器人示例。
当前反射没有 EntityReference；资产引用和字符串不是实体引用，不能猜测重映射。
具体边界与验证见[设计](../specs/2026-09-07-v0.10-prefab-design.md)和
[验收](../../verification/2026-09-07-v0.10-prefab.md)。本地完成不等于已合入 main。

## 下一包 10-10 综合验收

1. 审查并集成 Prefab 切片；对照 v0.10 八项能力建立完整验收清单和证据链接。
2. 使用现有 Game 规则、菜单/UI、角色动画/声音、物理与 Prefab 模板，形成能从
   菜单进入、完成一局、反馈胜负并重开的整合小样。Engine 不添加游戏规则。
3. 固定 Agent 任务：发现模板 → 实例化/调参 → 保存 → 暂停启动 → Step → 读快照/
   日志 → 判断 → Stop → Undo；覆盖事务失败与重开。验证 Human 等价操作。
4. 在生产 Editor 和独立 Application 验证 Game View 输入、布局、播放/暂停/停止、
   音频设备与实际呈现。把真实设备观察和 FakeRenderDevice 自动验收分开记录。
5. 记录同场景 CPU/Renderer 统计基线和预算；明确是否补充系统阶段 profiler，
   不把 CPU 总帧时间当成物理耗时或 GPU 耗时。完成两套预设构建、全量 CTest、
   文档对账后再评估 v0.10 完成条件与发布。

随后才进入 v0.11 Production Demo：完整 Roguelike、存档、多场景内容与联网需求
按产品路线重新估算。v0.10 当前尚未整体完成或发布。
