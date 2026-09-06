# main 进度调研与下一步开发路线建议

日期：2026-09-06。调研基线：`main` / `origin/main`，`273e35b418b8c0bc38ef9d7d8f58654299d2f62d`。

原调研状态（2026-09-06）：开发路线提案；当次只更新文档，不修改公共 API、引擎行为或当前 AGENTS 里程碑。v0.10 的完整范围继续以[版本路线图](../../Janus%20Engine%20版本路线图.md)为准，以下 A–F 是实施阶段，不是新的发布版本。

后续执行更新（2026-09-07）：用户已确认此路线，首个项目配置/Input 切片在 `codex/v0.10-project-input` 完成实现与本地验证，尚未合并。原调研结果和验证记录保留为历史基线；新增契约见 [Stage A 专项设计](../specs/2026-09-06-v0.10-project-input-design.md)，实现与验证见 [Stage A 记录](../../verification/2026-09-07-v0.10-project-input.md)。10-03 共享 Runtime 调度及 B–F 仍为后续任务。

## 1. 建议与产品目标

建议结束 v0.9 的集成收尾，进入 **v0.10 Game Systems**。用一个单机卡牌战斗小样贯穿输入、UI、动画和音频，再以独立物理验证场景补齐 Physics，最后完成 Prefab 与综合验收。小样只服务引擎验证；完整 Roguelike、存档、多场景内容和联网仍属于后续工作。

下一步的核心结果应是：Human 能制作并运行一个有菜单、可点击卡牌、数值反馈、动画和声音的小型游戏；Agent 能通过同源能力配置内容、读取运行结果并验证修改。继续增加远程 MCP、泛化任务系统或高级 Profiler，不是当前关键路径。

推荐第一个开发包是 **项目配置 + Action Input 的完整纵向切片**：加载配置 → Lua 按 Action 查询 → Editor Game View 操作 → 保存重开 → 自动测试。不要一次启动全部 Game Systems。

## 2. 进度与证据

证据优先级：当前代码和 Git/CI 状态 → 可复查的验证记录 → 版本文档中的状态描述。历史验证不等于本次复测，也不等于 Release。

| 项目 | 调研结论 | 证据 |
|---|---|---|
| main 同步 | 从 `fc6ae00` 快进到 `273e35b`；开始时工作区干净 | `git fetch origin`、`git switch main`、`git pull --ff-only origin main` |
| v0.1–v0.8 | 已有引擎、渲染、场景、资源、Lua、Editor、Reflection/Command、MCP 基础 | [路线图](../../Janus%20Engine%20版本路线图.md)、Engine/Editor/MCP 与 Tests |
| v0.9 | 已合入 main，不能继续标注“待集成” | [PR #81](https://github.com/ULookup/Janus/pull/81)，2026-09-06 15:32:45 UTC 合并；PR 的 Windows 检查成功 |
| v0.9 验证 | 历史本机 287/287；真实 Editor/stdio、事务、故障现场和 Profiler 有记录 | [v0.9 验证记录](../../verification/2026-09-06-v0.9-agent-development-loop.md) |
| 发布 | CMake 版本为 0.9.0；查询时 `gh release list` 无条目 | 根 CMakeLists.txt；GitHub Releases 查询；版本号不代表已发布 |
| Production Demo | 仓库只有 Sandbox/SandboxProject，尚无 Game 目录 | Git 跟踪文件清单；完整游戏未开始，不能把 Sandbox 算作 v0.11 |

本次本机复测和 main CI 的最终结果记录在本文末尾。

### 已具备的可复用基础

- `Engine/Runtime/RuntimeSession.*`：Stopped/Playing/Paused/Faulted、隔离 RuntimeScene、固定 1/60 秒中性输入 Step。
- `Engine/Core/Log/LogStore.*`、`Engine/Core/Profiling/CpuProfiler.*`：有界日志与 CPU 帧记录；Editor 与 MCP 共享诊断。
- `Engine/Core/Command/CommandBus.*`、`Editor/ProjectSession.*`：共享作者态命令、事务、回执、失败恢复与运行态写保护。
- `Engine/Scene/SceneReflection.*`：Transform、SpriteRenderer、Camera、LuaScript 的 metadata，支持持久化、Inspector 和 MCP。
- `MCP/Resources/DebugResources.cpp`、`MCP/Tools/TransactionTools.cpp`、`Tests/MCP/mcp_external_e2e.py`：运行调试、事务与 modern/legacy 子进程回归。

### 进入 Game Systems 前必须看见的缺口

| 缺口 | 当前实现证据 | 对路线的影响 |
|---|---|---|
| 项目设置尚非完整项目模型 | `Engine/Application/ApplicationConfig.h` 使用 ProjectRuntimeConfig，默认 `Scenes/Battle.scene`；无已实现的 project.json 工作流 | 先明确 manifest、默认值、旧项目兼容、保存和生效时机 |
| Gameplay Input 只有键盘底层状态 | `Engine/Core/Event/Event.h` 无鼠标事件；`InputState.h` 只有按键查询；Lua 直接查 key | Action Mapping 与游戏指针事件必须在 Button 前完成；Editor ImGui 鼠标操作不等于 Game UI 输入 |
| 资源类型仅三种 | `Engine/Asset/AssetMetadata.h`：Texture、ShaderSource、LuaScript | AnimationClip、AudioClip、Font/Prefab 的格式与加载需随需求逐项补齐 |
| 新游戏系统尚未实现 | `Engine/Scene/Components.h` 无 Animator、UI、Audio、Physics 组件 | 必须把 Runtime、Reflection、持久化、Editor、Agent 和测试一起交付 |
| Runtime 调度存在两条路径 | Editor 使用 RuntimeSession；`Engine/Application/Application.cpp` 的 managed runtime 直接创建/更新 ScriptEngine | 加系统前统一共享的运行阶段，否则 Editor 和独立游戏可能出现行为分叉 |
| Agent 能读反射组件，不等于能读任意 Lua 游戏状态 | runtime/entity 使用 Reflection；不存在内置 Health 组件或任意 Lua table 调试接口 | 卡牌伤害验证需要明确的 Game 状态/诊断契约，不应把 Health 写进 Engine |
| 制作流程仍有产品欠账 | 当前 Scene Tools 无 Duplicate/Reparent；Asset Resource 按 UUID 读，非 asset.search/import 工作流 | 按 UI 层级、资源发现与 Prefab 的实际需求补齐；其余明确归入 v1.0 前对账 |

上述结论是实现范围核对，不代表已复现代码缺陷。

## 3. 文档冲突与处理建议

| 不一致 | 处理方式 |
|---|---|
| README/路线图称 v0.9 在开发分支或待集成，但 PR 已合并 | 本次同步事实状态；历史验证记录保留原始基线，并追加集成说明 |
| PRD §4 将 Animation/UI/Audio/Physics 列为产品 P0；路线图 §18 将部分系统列 P1 | 产品必要性与施工顺序混用。依 PRD §54 的约定，以路线图确定版本边界；不据此删减 v0.10 系统 |
| PRD §43 希望 Lua 错误后 Runtime 尽量继续；v0.9 明确采用 Faulted 停止推进并保留现场 | 本路线保留已实现的 Faulted 契约；若要脚本级隔离，另做错误策略设计与兼容测试 |
| PRD 提及测试入口、Headless、资产操作、分级权限；后续版本的必交付列表覆盖不完整 | 建立 v1.0 前产品对账项，不能因 v0.9 完成宣称全 PRD 完成；也不能将全部欠账塞入 v0.10 |
| 路线图 v0.11 建议联网卡牌 Demo；PRD 强调先保证真实游戏闭环，且完整 Multiplayer 非目标 | 推荐先单机验收，再做最小联网验证；联网是否为 v0.11 发布硬门槛需在该版本立项时明确，本文不擅自删除 |
| AGENTS 仍以 v0.9 为当前里程碑 | 本次不直接改为“v0.10 开发中”；启动首个实现任务时同步 AGENTS、路线图和设计状态 |

## 4. v0.10 分阶段路线

依赖顺序：**A 项目与输入 → B 最小可玩 UI → C 动画与音频 → D 物理 → E Prefab → F 综合验收**。

### A. 项目配置与输入纵向切片（最高优先级）

交付内容：

- 最小版本化项目配置：名称、默认 Scene、资源/脚本路径、分辨率、VSync、Target FPS 和 Action Bindings。先定义 Target FPS 与 VSync 的优先关系；旧 SandboxProject 无 manifest 时保留兼容默认值。
- 配置经 Engine API 加载/验证；Editor 只提供必要设置入口。磁盘保存明确独立于 Scene Transaction，活跃事务或 Runtime 下的修改策略必须在 session 入口执行。
- Action 支持 down/pressed/released、多按键映射与未知名称错误策略；迁移 PlayerController，保留旧 Lua key API 兼容。
- 为下一阶段添加后端无关的指针移动/按键、焦点丢失处理和 Game View 坐标映射，避免 UI 和世界同时消费一次点击。
- 明确共享 Runtime 执行阶段，逐步收敛 managed Application 与 RuntimeSession。只抽取真实复用，不引入通用调度框架。

验收：将 MoveLeft 从 A 改绑为左箭头，保存重开后生效，Lua 不改；旧 SandboxProject 仍可运行；按下/释放/重复事件和失焦测试通过；Paused Step 仍使用中性输入，作者态不受运行修改污染。

### B. 最小可玩 UI 与 Lua 游戏小样

交付 Canvas、Panel、Image、Text、Button，最小 Anchor/Pivot/层级/布局与点击事件。Game UI 是可在 Runtime 独立运行的引擎能力；ImGui 继续只承担 Editor 工具界面。

先完成单一字体、字形资源和确定的文本布局范围，明确字符集、换行、裁剪、DPI 与缩放行为；不做富文本、复杂字体回退或通用 UI 编辑器。字体栅格化依赖需要独立决策记录，不能假定现有 SpriteRenderer 已具备 Text 能力。

若 UI 依赖 Hierarchy 调整，同一阶段补最小 Reparent Command、Human/MCP 入口及 cycle/UUID/Undo 测试。资源发现提供按类型/名称的只读查询；新增操作必须显式进入权限 whitelist，不能因为 read-only 就跳过分类。

在 `Game/` 放一个固定单机战斗：菜单 → 出牌 → 伤害/血量反馈 → 胜负 → 重开。卡牌、Health、回合规则全部由 Game/Lua 拥有。为自动测试与 Agent 读取选择一个最小结构化游戏诊断契约，禁止任意执行 Lua 或把游戏字段搬入 Engine。

验收：鼠标和 Confirm 可完成一局；点击 UI 不触发世界操作；相同输入在 Editor Runtime 与独立运行入口得到相同战斗结果；Human 与 Agent 配置 UI 都可保存、Undo，Agent 能读取伤害结果。

### C. Sprite Animation 与基础 Audio

AnimationClip 使用资源 Handle、帧/UV、时长和 Loop；Animator 支持 Play/Stop/Switch。运行播放进度独立于持久化 authoring 数据，Pause/Step/Stop 与 v0.9 状态机一致，不引入动画状态机编辑器。

Audio 支持 Clip/Source、Play/Pause/Stop、Volume/Loop；先验证出牌音效和背景循环。设备由明确的 Engine host/runtime 生命周期持有；设备缺失可诊断，失败不会使项目打不开。测试使用 fake/null device，真实声音需要本机验收。

验收：出牌时播放一次反馈动画/音效，暂停和停止行为有定义；动画单步可复现；重复 Play/Stop 无资源遗留；缺资源有可读错误；Agent 可读取必要播放状态，不能把 runtime cursor 写入 Scene。

### D. Physics 2D 最小封装

依现有架构包装 Box2D，首轮只做 RigidBody2D、Collider2D、Trigger、Raycast 和 Collision Event。先约定单位、固定步长、Transform 同步、父子层级/非均匀缩放支持范围及延迟销毁规则。

固定物理 tick 与当前变量 Update 的关系必须显式设计：限定累积补步，防止追帧失控；MCP Step 保持 1/60 秒语义。Physics/Lua/Animation 的顺序通过测试固定，不在多个 host 重写。

验收使用独立落体/阻挡/Trigger 场景，覆盖 Raycast、Lua 回调中销毁、Pause/Step、Stop/重启和 authoring 隔离。卡牌小样对碰撞需求较低，因此物理不阻塞首个可玩阶段，但它仍是 v0.10 完成条件。

### E. Prefab Foundation 与制作效率

第一版建议定义为“可复用实体子树模板”：保存模板 → 实例化 → UUID/内部引用重映射 → 同组 Undo。明确模板更新暂不自动传播；若 PM 要求持续连接的实例与 override，必须重新估算，不能把普通 Clone 称为完整 Prefab。

复用 Scene Reflection/Command，不复制序列化字段表。若存在 Game View/UI 的重复卡牌需求，先用 Duplicate 完成小样，再交付可持久化模板。模板文件写入不纳入 Scene 事务，实例化产生的作者态修改可以纳入事务。

验收：生成三份对象无 UUID 冲突；内部引用指向各自实例，外部资源 Handle 保持不变；保存重开后等价；一次 Undo 撤销一次实例化；Agent 与 Human 通过同一个 Engine 入口创建实例。

### F. 完整验收与 v0.11 交接

- 所有 v0.10 新 authoring 组件完成 Reflection、Scene 保存/克隆、Inspector、Command/Undo、MCP Schema 贯通；运行状态采用只读快照，不强塞进作者态属性。
- 同时验收小型卡牌游戏和 Physics 验证场景，覆盖 v0.10 全部八项范围。
- 固定 Agent 任务：发现资源并配置对象、修复一次游戏参数问题、读取运行反馈、定位一项渲染统计问题。记录环境、次数、成功率、人工介入和失败原因；确定性协议测试不能替代真实 Agent 成功率。
- 保留 modern/legacy stdio、权限默认拒绝、主线程调度、事务限额/补偿与 stdout purity 回归。
- 记录 Animation/Physics CPU scopes、UI/游戏渲染统计与资源重复加载基线；不将 CPU timing 描述为 GPU timing。

## 5. 建议工作包与合并策略

| 工作包 | 依赖 | 可独立审查的交付 |
|---|---|---|
| 10-00 | main 基线 | v0.10 专项设计：小样验收脚本、项目格式、Runtime 顺序、依赖决策和兼容边界 |
| 10-01 | 10-00 | 项目配置加载/保存、旧项目兼容、Editor 最小设置入口及测试 |
| 10-02 | 10-01 | Action Input、Lua 迁移、指针/焦点/Game View 映射及端到端验证 |
| 10-03 | 10-02 | 共享 Runtime 阶段与独立入口一致性测试，不改变现有 Step 契约 |
| 10-04 | 10-03 | 最小 UI 层级/布局/Image/Text，字体资源与加载边界 |
| 10-05 | 10-04 | Button/Lua、必要 Reparent 与资源发现、单局游戏和可读诊断 |
| 10-06 | 10-05 | AnimationClip/Animator 完整纵向切片 |
| 10-07 | 10-06 | AudioClip/AudioSource 完整纵向切片与设备验证 |
| 10-08 | 10-07 | Physics 封装、固定 tick 和独立验证场景 |
| 10-09 | 10-08 | Prefab 模板与实例化、UUID/引用/Undo 验收 |
| 10-10 | 10-09 | 综合 Demo、Agent benchmark、性能记录与文档收口 |

每包先添加有意义的失败测试，再实现，先跑相关测试后跑全量 preset。串行推进；若单包过大，可拆成保留可运行入口的连续 PR，不能把“接口空壳”当作阶段完成。

暂不承诺日历工期。10-00/10-01 完成后，用实际周期估算剩余工作；字体/UI、Runtime 调度和 Prefab 语义是主要不确定项。发生延期优先收窄内容量、编辑器便利功能和资源格式覆盖，不删掉已经声明的八项 v0.10 能力。

## 6. 新依赖与兼容门槛

本提案不新增依赖，也不虚构版本 pin。Box2D 来自现有架构要求；miniaudio 是现有文档候选；Text 所需字体方案待 10-00 比较。

每个依赖落地前记录：解决的问题、精确 tag/commit、Windows/MSVC/CMake 支持、许可证与分发义务、PUBLIC/PRIVATE 边界、fake/headless 测试办法、对构建耗时和运行资源的影响。第三方原生类型不能穿过 Janus 公共 API；不要同时引入多套同类后端。

Scene v1 现有名称保持兼容；新增组件和资源格式必须测试旧文件、未知类型、非法值与缺资源。配置和 Asset 文件修改不冒充 Scene Transaction 的可回滚部分。Runtime/设备/资源必须明确释放顺序，MCP worker 不得直接更新世界。

## 7. v0.11 / v1.0 产品对账

| 后续节点 | 推荐结果 | 尚需明确的边界 |
|---|---|---|
| v0.11 Production Demo | 把单局小样扩展为完整单机 Roguelike：多场景、存档、内容制作和可分发运行入口；执行路线图四项 Agent Benchmark | 联网作为第二阶段验证，是否为发布硬门槛在立项时明确；不提前建设 Engine replication 框架 |
| v1.0 Human 闭环 | 创建项目、打开/切换 Scene、配置/导入资源、制作游戏、调试、构建交付可复现 | 项目创建 UX、Asset Import/Reimport/Move、安全文件操作、Scene/Duplicate/Reparent 覆盖逐项对账 |
| v1.0 Agent 闭环 | 核心制作行为有 Agent 等价操作，游戏运行状态可读，修复可验证 | 正式测试入口、生产 Headless、分级权限/Dry Run、N 帧采样等 PRD 条目需明确“必交/缩减/延期”；测试 fixture 不算生产 Headless |
| v1.1+ | 依据真实使用问题选择扩展 | HTTP MCP、远程 Agent、通用 Agent Test Runner、高级网络、3D/Vulkan/Jobs 不进入当前施工范围 |

PRD 的版本摘要将详细边界委托给路线图，因此以上欠账需要在 v1.0 前形成显式范围决策；本次不直接修改产品承诺。

## 8. 原调研验证（2026-09-06）

本次只修改文档；构建测试用于确认调研基线。在 Visual Studio Developer PowerShell 环境执行，逐项检查完整配置、构建和 CTest 输出。

| 命令 / 检查 | 本次结果 |
|---|---|
| `cmake --preset windows-msvc-debug` | 成功 |
| `cmake --build --preset windows-msvc-debug --parallel 2` | 成功 |
| `cmake --preset windows-msvc-debug-tests` | 成功 |
| `cmake --build --preset windows-msvc-debug-tests --parallel 2` | 成功 |
| `ctest --preset windows-msvc-debug-tests` | 287/287 passed，0 failures，9.13 秒；包含 modern/legacy external stdio |
| `gh run view 34042646715` | main `273e35b` 的 [CI](https://github.com/ULookup/Janus/actions/runs/34042646715) completed / success |
| `git diff --check` | 通过 |
| 修改文档本地链接检查 | 14 个目标，0 失效 |
| `git check-ignore out/next-roadmap-ctest.log` | 生成日志被忽略 |

本次原始日志保留在 ignored `out/next-roadmap-*-configure.log`、`out/next-roadmap-*-build.log`、`out/next-roadmap-ctest.log`。构建仍报告既有 C4458（Reflection 名称隐藏）、C4996（getenv）、C4834（测试忽略 nodiscard）警告；configure 的可选 PkgConfig/LibUSB 未找到不阻塞 Windows preset。没有关闭 warnings 或修改源代码。

本次未重复真实 Editor 图形交互和设备测试；该部分只引用 v0.9 历史记录，不宣称完成 v0.10 或发布验收。最终工作区仅有本文与 README、版本路线图、v0.9 验证记录的文档变更，尚未提交或推送。
