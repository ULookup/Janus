# Janus 项目进度与产品需求对账

2026-09-08 C 包执行更新：#95 已合入 main（核查基线 `2fcfeab`）；#96 已合入
`codex/editor-safe-close`，其 B 包内容在本轮开始时尚未进入 main。开发分支
`codex/editor-move-gizmo` 集成两者后实现单对象 Move：只读子树预览、X/Y/平面手柄、
Ctrl 网格吸附、一次 Undo、取消与 Agent 竞争保护。两个 Debug preset 构建及本地回归通过，
原生拖动/窄窗口/最大化/保存重开已有证据；C 包尚未提交/创建 PR。
详见[Move 验证](verification/2026-09-08-editor-move-gizmo.md)。D–F、目标系统 DPI/设备矩阵、
PM-01 完整原生出口及首次用户验收仍待完成；v0.10 未发布。
以下 A/B “尚未提交”与旧 main 描述保留其记录时点，以本段 Git 状态更新为准。

2026-09-08 设计补充：最新远端 main 已到 `df00a14`，#93 的工作区/资源赋值/图标与
#94 的视觉优化均已合入。下文 `7ecdfb8` 表格保留 09-07 核对时点，不能将其中的
“本分支待集成”、Image/Animator 无入口或未见选中轮廓当作最新代码事实。
最新 main 的[合并后 CI](https://github.com/ULookup/Janus/actions/runs/34202589819)已通过：
417/417、50.43 秒，测试于北京时间 09-08 16:12:11 完成；v0.10 仍未发布。
新增[发布准备实现设计](superpowers/specs/2026-09-08-v0.10-release-readiness-design.md)与
[增量实施计划](superpowers/plans/2026-09-08-v0.10-release-readiness-plan.md)，细化关闭保护、
Duplicate/Move、资源与偏好、文件生命周期及 Release；逐包执行状态见下，不代表整个方案通过发布验收。

2026-09-08 A 包执行补充：`codex/editor-safe-close` 已本地实现关闭协商、保存/放弃/取消、
设置草稿与 Runtime/事务保护；Debug CTest 427/427 和两种生产 stdio 集成回归通过。
尚未提交/合并；X/Alt+F4/弹窗焦点布局的原生 PM 验收仍待完成，因此 PM-01 尚不标为关闭。
详见[安全关闭验证](verification/2026-09-08-editor-safe-close.md)。

2026-09-08 B 包执行补充：当前工作区已实现共享 Duplicate 命令、Ctrl+D/MCP 入口，
六类资源槽搜索/定位/拖入、可读 Prefab 名称和 create-only 原子写入；中文路径显式按 UTF-8 保存。
Debug CTest 439/439 通过，本机原生 Hero 复制、Animator/Image 赋值、命名导出与保存重开已验证。
PM-05/06 对应缺口已有本机修复证据；尚未提交/合并，Release/DPI/首次用户验收仍保留。
详见[复制与资源工作流验证](verification/2026-09-08-editor-duplicate-assets.md)。C–F 尚未实施。

核对日期：2026-09-07（北京时间）；代码基线：main / origin/main `7ecdfb8`。
本文是当前实现、集成、验证及待办的统一入口。PRD 定义产品目标，版本路线图定义版本范围，
专项设计定义实现契约，日期化计划与验收记录保留当时证据；这些文档不再各自推断最新 Git 状态。
后续更新本文时必须重新核对基线及 CI，不能把本日期的结果套用到新提交。

## 1. 当前结论与集成状态

**v0.10 Game Systems 的计划切片及综合示例已合入 main，当前处于发布准备阶段。**
尚未发布 v0.10，也不能据此宣称全 PRD 或 v0.11 Production Demo 已完成。
代码具备的能力、Human 面板覆盖、自动验收和发布验收需要分别判断。

后续[原生 PM 验收](verification/2026-09-07-pm-editor-acceptance.md)已实际运行最新 Editor：
综合示例与基础制作闭环通过，但发现未保存场景直接关闭会丢失改动（PM-01 / P1）。
当前不建议通过编辑器日常制作及发布验收。随后按用户参考图进行的本地 UI 升级已补齐
工作区分区、首次取景/聚焦、DPI 缩放和类型约束资源选择；这是本分支实现，集成状态以 Git/PR 为准，
不计入上表 main 基线，也未解决 PM-01。具体证据与限制见
[UI 升级验收](verification/2026-09-07-editor-ui-upgrade.md)。
随后补充了 40 枚同源 SVG/原生矢量图标，并接入工具栏、栏目、组件和资源类型；
见[图标套件验收](verification/2026-09-07-editor-icons.md)，同样属于本分支待集成实现。

| 项目 | 核对结果 | 证据 |
| --- | --- | --- |
| main 基线 | `7ecdfb86395672b206bb8d6e70c70cf0f7172734`，本地与远端一致 | `git log`、`git ls-remote origin refs/heads/main` |
| 集成 PR | #92 于 2026-09-07 14:36:08 合入 main；实现提交 `ec728e3` | [PR #92](https://github.com/ULookup/Janus/pull/92) |
| 依赖集成 | UI/Text 随 #85，Button/Combat/Animation 随 #91 依赖链，Audio #89、Physics #90，均已进入上述 main | main Git 历史；`b9b990b` 是 #92 的父基线，不是当前 main |
| PR 检查 | Windows 检查成功 | [PR CI](https://github.com/ULookup/Janus/actions/runs/34090993688) |
| 合并后 main 检查 | configure、build、CTest 成功；412/412，47.77 秒；任务于 14:44:11 完成 | [main CI](https://github.com/ULookup/Janus/actions/runs/34091574394) |
| 开放 PR | 查询时为 0；不据此推断没有未提交或未立项工作 | `gh pr list --state open` |
| 版本与发布 | CMake 仍为 `0.9.0`，MCP server version 取自该值；远端无 Tag，GitHub 无 Release | [CMake](../CMakeLists.txt)、[版本注入](../Editor/CMakeLists.txt)、`gh release list`、`git ls-remote origin 'refs/tags/*'` |

## 2. 里程碑与实现证据

v0.1–v0.8 已完成引擎基础、Renderer2D、ECS/Scene、Asset/Serialization、Lua、Editor、
Reflection/Command 与原生 MCP。v0.9 已通过 #81 合入 main，补齐 Runtime 调试、共享诊断、
事务及 Agent Activity；历史本地回归为 287/287，见[v0.9 验收](verification/2026-09-06-v0.9-agent-development-loop.md)。

下表所有 v0.10 工作包都已在当前 main 中；“已集成”不表示未列入测试的产品体验也已完成。

| 工作包 | 当前交付与边界 | 实现 / 验证入口 |
| --- | --- | --- |
| 10-01 / 10-02 项目与输入 | versioned project.json、原子保存、旧项目默认值、Action 聚合、Game View 输入与逻辑坐标；设置保存独立于 Scene Undo | [ProjectSettings](../Engine/Project/ProjectSettings.h)、[验收](verification/2026-09-07-v0.10-project-input.md) |
| 10-03 共享 Runtime | Application 与 RuntimeSession 共用 RuntimeExecution；宿主保留各自生命周期与故障策略 | [执行路径](../Engine/Runtime/RuntimeExecution.cpp)、[验收](verification/2026-09-07-v0.10-shared-runtime.md) |
| 10-04a UI | 单根 Canvas、UIRect/Panel/Image、Anchor/Pivot、裁剪与顺序绘制、可撤销 Reparent；不支持多/嵌套 Canvas | [布局](../Engine/UI/UILayout.cpp)、[验收](verification/2026-09-07-v0.10-ui-layout.md) |
| 10-04b Text | Font 离线图集、UTF-8、换行/对齐、Lua 文本更新、assets.search；无 shaping/自动换行/完整字符覆盖 | [文本布局](../Engine/UI/TextLayout.cpp)、[验收](verification/2026-09-07-v0.10-ui-text.md) |
| 10-05a Button | 有序指针事件、键盘焦点、捕获取消、输入消费、同实体 Lua OnClick；中性 Step 不点击 | [交互](../Engine/UI/UIInteraction.cpp)、[测试](../Tests/UI/ButtonTests.cpp) |
| 10-05b Combat | Game Lua 固定卡牌规则、胜负/重开、显式有界标量 snapshot；读取快照不执行 Lua | [Combat.lua](../Game/Scripts/Combat.lua)、[验收](verification/2026-09-07-v0.10-playable-combat.md) |
| 10-06 Animation | Clip/Animator、循环/单次、Play/Stop/Switch、运行态 pose；无状态机/动画编辑器；Human 资源赋值缺口见下表 | [AnimationSystem](../Engine/Animation/AnimationSystem.cpp)、[测试](../Tests/Animation/AnimationTests.cpp) |
| 10-07 Audio | PCM16 WAV、AudioSource、Lua 控制、SDL 惰性设备、暂停清队列与静音 Step；无流式/空间/DSP | [AudioSystem](../Engine/Audio/AudioSystem.cpp)、[测试](../Tests/Audio/AudioTests.cpp) |
| 10-08 Physics | Box2D 3.1.1、刚体/矩形碰撞体、Trigger/Raycast、延迟销毁；1/60 tick、4 substeps、最多 8 catch-up ticks；根实体单位缩放 | [PhysicsSystem](../Engine/Physics/PhysicsSystem.cpp)、[测试](../Tests/Physics/PhysicsTests.cpp) |
| 10-09 Prefab | 有界单根模板、UUID 重映射、共享命令、Undo/Redo/事务、原子 registry 发布；仅作者态展开，无关联/覆盖/运行时生成 | [Prefab](../Engine/Prefab/Prefab.cpp)、[测试](../Tests/Scene/PrefabTests.cpp) |
| 10-10 综合验收 | 默认 Integrated 场景组合上述系统；Human/Agent authoring、双宿主一致性、投影和 profiler 回归；修复 Combat 热重载后 Arena 引用失效 | [集成测试](../Tests/Game/IntegratedTests.cpp)、[验收](verification/2026-09-07-v0.10-integrated-acceptance.md)、[复审](verification/2026-09-07-v0.10-integration-review.md) |

Game 默认加载 `Scenes/Integrated.scene`，18 个作者态实体、3 个物理体、两个展开的 Fighter
子树。三次 Strike 胜利，三次 Wait 失败；Arena 组合落地/受击反馈，伤害仍由 Combat 结算。
IntegratedVerification 调用相同规则入口，480 个中性 Step 覆盖胜利、重开和失败；不是 MCP
输入注入，也不替代真实按钮操作测试。运行方式见[Game README](../Game/README.md)。

## 3. PRD 对账：产品入口与剩余范围

“已具备”表示表内明确能力存在；“部分”表示关键入口或验收仍缺失；“待范围决策”不等于
批准延期。以下建议归属沿用既有路线，不把所有 PRD 欠账自动追加到 v0.10。

| PRD / 产品项 | 当前事实与证据 | 状态 / 后续处理 |
| --- | --- | --- |
| §7、§45、§46 创建/打开项目 | [EditorLaunchOptions](../Editor/EditorLaunchOptions.cpp) 支持现有项目路径及默认 SandboxProject；[ProjectSession](../Editor/ProjectSession.h) 提供 Open，无项目创建向导/模板生成入口 | 部分；v1.0 Human 闭环对账，不能把“打开已有项目”算作 Create Project |
| §8、§13 场景制作 | Entity Create/Delete/Rename/Reparent、属性/组件命令、Scene Save 已具备；[EditorActions](../Editor/EditorActions.h) 无 Duplicate；[ProjectSession](../Editor/ProjectSession.h) 无普通新建/切换 Scene API，DiscardUnsavedAndReload 是恢复操作 | 部分；v0.11 多场景需求及 v1.0 制作流程需明确交付；ScenePicker 是 Sprite 拾取，不是场景文件选择器 |
| §10 Scene View | [EditorApplication](../Editor/EditorApplication.cpp) 有平移、缩放、网格和点击拾取；Transform 可在 Inspector 修改；未见 Move Gizmo 或选中轮廓绘制入口 | 部分；区分“可修改位置”与“视图内拖拽移动”，纳入 Human UX 对账 |
| §14–15、§27–28 资产工作流 | [AssetBrowserPanel](../Editor/Panels/AssetBrowserPanel.cpp) 枚举 registry、部分类型筛选、元数据展示及部分组件赋值；[assets.search](../MCP/Tools/SceneTools.cpp) 按名称/类型有界查询；未见通用 Import/Reimport/Rename/Move/Delete、目录浏览及依赖/引用图入口 | 部分；v0.11 内容制作/v1.0 对账，Prefab Export 不等于通用 Import；[AssetType](../Engine/Asset/AssetMetadata.h) 当前不含 Scene |
| §18–19 Human 资源赋值 | main 基线对 AssetReference 只显示 UUID；本地 UI 升级已在 [InspectorPanel](../Editor/Panels/InspectorPanel.cpp) 加入匹配类型的注册资源选择，覆盖 Image.texture、Animator.clip 等资源槽；[AssetBrowserPanel](../Editor/Panels/AssetBrowserPanel.cpp) 增加 AnimationClip 筛选与 Animator 赋值 | 本分支实现待审查与集成；不将本轮 UI 变化描述为 main 或 Release 已交付 |
| §16–19、§41–42 Lua/UI/配置/输入 | 已实现上述纵向切片；字体、动画、音频、物理和 Prefab 的格式/运行边界见专项设计 | 已具备当前 v0.10 范围；高级编辑器和格式扩展不是本轮待实现系统 |
| §24–26、§29–30 Agent 制作/观察 | UUID 场景资源、Reflection 属性 schema、共享命令、运行控制及运行实体/快照已具备；没有通用 scene.create/open/duplicate 工具 | 部分产品覆盖；Game HP 通过显式 snapshot 读取，不是任意 Lua 表或 Engine Health 组件 |
| §20–22、§31–32 诊断 | [DebugResources](../MCP/Resources/DebugResources.cpp) 有 logs/recent 的 after/limit/level/category/runtimeId 和 profiler 的 latest/frameId；CPU scope 与渲染计数同源 | 已具备基础；N 帧主动 Capture 和按时间范围日志接口仍需范围决策，逐帧读取有界历史不等于主动 Capture |
| §33 Agent Testing | [Tests/CMakeLists.txt](../Tests/CMakeLists.txt) 有 Catch2/CTest、双协议外部 E2E；生产 MCP 未注册 List/Run Tests、Build 工具 | 部分；测试基础设施已存在，正式 Agent 测试入口仍待范围决策 |
| §34–37 Command/Undo/Activity/Transaction | [ProjectSession](../Editor/ProjectSession.h) 统一作者态入口，独占事务、限额、逆序补偿、RecoveryRequired；Save/Runtime 不在事务内 | 已具备当前契约；不是磁盘事务或运行世界回滚 |
| §38–39 Dry Run / 权限分级 | [McpPermissionPolicy](../MCP/Host/McpPermissionPolicy.cpp) 对未分类操作拒绝，生产 Editor 使用 AllowAll policy；有分类/授权 seam，无 PRD 四级用户策略和 Dry Run 产品入口 | 待范围决策；不能将白名单分类描述为默认 ReadOnly 或完整分级权限 |
| §40 Headless | [启动解析](../Editor/EditorLaunchOptions.cpp) 仅有 --project/--mcp-stdio 等现有参数；生产入口仍初始化 Editor；FakeRenderDevice 外部 host 位于 Tests | 未交付生产 Headless；v1.0 前决定最小能力或明确延期，测试 fixture 不计作生产实现 |
| §43 故障策略 | [RuntimeSession](../Engine/Runtime/RuntimeSession.cpp) 遇执行失败进入 Faulted、保留现场直到 Stop；[Audio](../Engine/Audio/AudioSystem.cpp) 设备失败可静音继续，内容错误正常失败 | 与 PRD“脚本错误尽量继续”存在语义差异；沿用已验收契约，未来若改为单脚本隔离需独立设计与回归 |
| §55 Demo、§57–58 Agent Benchmark | 固定单局与集成 authoring/Step 回归已具备；未见完整 Roguelike、存档/多场景流程、联网 Demo 或路线图 A–D 任务成功率/失败模式报告 | v0.11 Production Demo；不能用通过 412 个测试替代真实 Agent 成功率指标 |
| §59–61 性能/平台/交付 | 有 Renderer/ECS/CPU benchmark 与 Windows MSVC CI；[预设](../CMakePresets.json) 仅 Debug 系列，[CI](../.github/workflows/ci.yml) 仅 Debug tests，无 Release 预设或打包流程 | 发布准备；真实目标设备/GPU/Present 性能尚无本轮新证据，不作跨平台或稳定 60 FPS 承诺 |

此表确认了制作入口与规划覆盖差异，不是对全仓库所有错误路径的审计，也未修改任何产品承诺。

## 4. 当前架构与依赖事实

- `ProjectSession` 显式拥有 ReflectionRegistry、CommandBus 与作者态，Human 经 EditorActions、
  MCP 经 owner-thread dispatcher/permission/shared commands 进入同一受保护路径。
- `RuntimeExecution` 借用 Scene/AssetService/可选 CpuProfiler，拥有输入快照、ScriptEngine、
  UIInteraction、Physics/Animation/Audio；正常 Advance 为 UI → Reload → Lua → Physics →
  Animation → Audio。Physics 在 Lua OnCreate 前启动，停止先完成 Lua 清理再释放系统资源。
- RuntimeSession 管理 Clone、Paused/Faulted 保留和 Stop；独立 Application 共用执行阶段，
  不强制采用 Editor 的故障世界保留模型。Step 为 1/60、neutral input、跳过重载和 UI 派发。
- 持久化身份使用 UUID；Scene v1 与 Prefab 共用 active Reflection 和子树快照，不保存 GPU、
  Lua、音频游标或物理世界 ID。Game 卡牌/HP 规则不进入 Engine。

以下版本来自当前 [CMake 声明](../CMakeLists.txt)，是已采用依赖，不是最新版本推荐。

| 依赖 | 当前固定版本 / 身份 | 用途与边界 |
| --- | --- | --- |
| SDL | 3.4.14 | Windows 窗口/输入/音频后端；原生类型留在实现侧 |
| Box2D | 3.1.1，commit `8c661469c9507d3ad6fbd2fea3f1aa71669c2fe3`，归档 SHA256 固定 | Engine PRIVATE，单线程 2D solver |
| Lua | 5.4.9，归档 SHA256 固定 | JanusLua；Game 脚本运行 |
| glad / glm | 2.0.8 / 1.0.3 | OpenGL 4.5 loader / 数学实现 |
| spdlog / nlohmann_json | 1.15.3 / 3.11.3 | 日志 / JSON 数据与协议 |
| stb | commit `2c980bb59875b0d32144a71867fbdebb2f77cd20` | 图片加载 |
| Dear ImGui / Catch2 | 1.92.9b / 3.15.3 | Editor UI / 自动测试 |

架构文档早期提到的 miniaudio、PhysicsService、Network/Headless 是候选或目标模型；当前实际
使用 SDL AudioDevice、PhysicsSystem，未引入 miniaudio 或 Engine 网络系统。
项目自身尚未选择开源 License；本轮不选择许可，分发前按现有仓库规则处理。

## 5. 验证证据与限制

集成前本地复审：两套 Debug configure/build 成功，`[acceptance]` 6 项 / 1789 条断言，
CTest 412/412、32.30 秒，见[复审记录](verification/2026-09-07-v0.10-integration-review.md)。
合并后 main CI：412/412、47.77 秒；二者是不同环境与时点，不能混用耗时。

此前[原生验收](verification/2026-09-07-v0.10-integrated-acceptance.md)包含真实 Editor/Sandbox
输入及渲染、modern/legacy 生产 stdio、真实音频设备 open/submit/clear/close。音频未做人工
音质试听。CPU 基线为 i9-14900HX / MSVC 14.38 / Debug / FakeRenderDevice / dummy audio，
暖机 180、测量 720 帧：模拟+渲染提交 median 1.706 ms、p95 2.087 ms，最多 309 sprites、
15 draw calls。它不含 GPU、Present、驱动等待、输入轮询和 client 更新，不能作为整帧保证。

本次复验命令、结果及文档检查记录见[PM 核对验证](verification/2026-09-07-project-status-audit.md)。
前轮核对仅更新文档；随后按用户要求完成了[原生 PM 验收](verification/2026-09-07-pm-editor-acceptance.md)，
覆盖真实鼠标/数字键胜负、暂停/单步/停止、命令与 Prefab、保存重开及未保存关闭。
音质/GPU/Release 仍未复验；本轮未修改实现，也未新增实现测试来证明文本修改。

## 6. 下一步及验收出口

首个 Release 的 Human 体验优化已有[产品设计](superpowers/specs/2026-09-07-first-release-editor-ux-design.md)
和[实施计划](superpowers/plans/2026-09-07-first-release-editor-ux-plan.md)：覆盖安全退出、栏目布局/DPI、
资源赋值、取景与基本场景生命周期，建议按 R1–R8 顺序完成。它们是待实施方案，未改变当前完成状态。

| 顺序 | 工作 | 完成条件 | 当前状态 |
| --- | --- | --- | --- |
| 1 | 文档状态收口 | 当前入口统一到 `7ecdfb8`，历史记录标注时点，PRD 对账有证据及待决项 | 本轮更新 |
| 1a | 关闭时保护未保存场景 | PM-01：Save / Discard / Cancel、保存失败保留现场，覆盖各退出入口 | 原生验收失败；发布前优先修复 |
| 2 | Human 制作入口核对 | 对 Image/Animator 资源赋值及相关文档作明确处理；修复应先补交互/命令测试，或明确版本限制与实际可用替代流程 | 待处理，不在本轮改代码 |
| 3 | Release/目标设备验证 | 确定 Release 配置与可复现交付入口；运行相关全量回归、原生设备验收，区分 CPU/GPU/Present 指标 | 待完成 |
| 4 | v0.10 发布准备 | 更新 CMake/MCP 版本、Release notes、限制清单及分发/许可事项；发布提交的 CI 通过后再记录 Tag/Release | 未发布，本轮不改版本号 |
| 5 | v0.11 立项 | 明确完整游戏循环、存档、多场景、内容规模、Agent A–D 基准；明确联网是否是版本硬门槛 | 待范围确认，不自动启动 |
| 6 | v1.0 产品闭环 | 上述 PRD 欠账逐项“必交/缩减/延期”决策并关联验收；完整 Human 与 Agent 工作流都可复现 | 未完成 |

路线图 v0.11 建议联网卡牌 Roguelike，而 PRD 不要求完整 Multiplayer Engine。可先完成单机
再做最小联网验证，但这仍是实施建议，不是已批准删除联网目标。没有新增工期或完成百分比：
测试数量、模块数量与产品可用性不是同一个度量。
