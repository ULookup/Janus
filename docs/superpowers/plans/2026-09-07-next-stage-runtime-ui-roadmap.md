# 最新 main 调研与下一阶段开发路线：共享 Runtime → 可玩 UI

日期：2026-09-07（Asia/Shanghai）。调研基线：`origin/main`，`c8a7c06ace2c1161f49394cac91a51c41de58c6a`，即 PR #82 合并提交。

状态：下一阶段开发提案。本次只调整文档，不修改公共 API、引擎行为、第三方依赖或版本完成条件。沿用此前已确认的 [A–F 总路线](2026-09-06-next-development-roadmap.md)，本文更新最新进度并细化紧接着的 10-03～10-05；没有重新立项 Stage A，也没有开始实现 Stage B。

执行更新（2026-09-07）：用户随后确认执行。首轮 10-03 已在 `codex/v0.10-shared-runtime` 实现并完成本地验证，通过独立 PR 提交评审，尚未合并；共享 RuntimeExecution 已供两个宿主使用，308/308 CTest 通过，真实 Sandbox/Editor 启停及 Editor Play/Pause/Step/Stop 已验证。详见[专项设计](../specs/2026-09-07-v0.10-shared-runtime-design.md)和[验收记录](../../verification/2026-09-07-v0.10-shared-runtime.md)。下方 main 源码事实和第 9 节仍是实施前的调研基线；续轮 10-04a 已实现单 Canvas、UIRect、Panel/Image、CPU 裁剪、稳定绘制/几何命中顺序以及 Human/MCP Reparent；[专项设计](../specs/2026-09-07-v0.10-ui-layout-design.md)与[验收记录](../../verification/2026-09-07-v0.10-ui-layout.md)记录最终证据。当前下一包为 10-04b Text/字体资产，Button 和可玩单局仍未实现。

最新执行更新（2026-09-07，10-04b）：fetch 后 main 为 `c71ad50`（#83）；#84 已合入 shared-runtime 父分支但未进入该 main。10-04b 已在包含 main 与 UI 布局的 `codex/v0.10-ui-text` 本地实现，见[Text/字体设计](../specs/2026-09-07-v0.10-ui-text-design.md)和[验收记录](../../verification/2026-09-07-v0.10-ui-text.md)。本段覆盖上方旧的“尚未合并/下一包 Text”状态，下方调研表保留历史基线；当前下一包为 10-05a。


## 1. 建议决策

**下一阶段定为“共享 Runtime 执行阶段 + 最小可玩 UI”，先交付 10-03，再以连续完整切片完成 10-04/10-05。** 产品结果是一份可在 Editor 和独立入口运行的单机卡牌战斗小样：进入菜单、选择卡牌、出牌、显示伤害与血量、胜负、重开。Human 可以制作与游玩；Agent 可以通过同源 authoring 能力配置界面，并读取结构化运行结果。

理由：项目已经能编辑、运行、调试场景，Stage A 又补齐配置和输入；当前关键缺口是实际游戏交互。UI 前先收敛运行阶段，是为了避免随后把 UI、动画、物理的执行顺序和生命周期分别写进两个宿主。

实施边界：单一屏幕空间 Canvas、基本矩形布局、有限字体字形、鼠标和键盘操作、固定单局规则。不在本阶段扩展完整 Roguelike、存档、多场景流程、联网、通用 UI 编辑器、生产 Headless 或远程 MCP。这些仍分别归后续阶段及版本对账。

## 2. 最新进度与证据等级

证据采用 Git/源码/本次验证优先，历史验收补充。产品目标不等于实现，已合并不等于已发布。

| 范围 | 最新判断 | 依据与限制 |
|---|---|---|
| 工作区与 main | 调研开始时干净；保留 `codex/v0.10-project-input` 分支 | `git fetch origin main` 更新远端引用；HEAD 为 `9425ded`，与 main 的 tree 都是 `5e4ea35745213f8c0b11d6c853ffcac4c2bba8a7`，跟踪文件完全一致；未切分支或重置 |
| v0.1–v0.8 | 路线图标记完成，具备基础引擎及 Human/Agent authoring 链 | [版本路线图](../../Janus%20Engine%20版本路线图.md)；本次回归仍覆盖 ECS、资源、Scene、Lua、Editor、Reflection/Command 和 MCP |
| v0.9 | 已合入 main | [PR #81](https://github.com/ULookup/Janus/pull/81)；[历史验收](../../verification/2026-09-06-v0.9-agent-development-loop.md)及当前 Runtime/诊断/事务代码 |
| v0.10 Stage A | 已合入 main，不能再描述为待合并 | [PR #82](https://github.com/ULookup/Janus/pull/82)，2026-09-07 00:30:27 +08:00 合并；配置、Action、指针和 Game View 路由已实现 |
| Stage A 质量 | 历史真实 Editor/MCP 验收 + 本次自动复测通过 | [Stage A 记录](../../verification/2026-09-07-v0.10-project-input.md)；本次 299/299，0 failures；本次未重做真实图形/按键验收 |
| PR / main CI | PR #82 两项 Windows 检查成功；main 检查状态单独记录 | main [CI 34045665651](https://github.com/ULookup/Janus/actions/runs/34045665651)，最终查询结果见第 9 节，不能用 PR 检查替代 main 检查 |
| v0.10 剩余 | 10-03 及 UI、Animation/Animator、Audio、Physics、Prefab 尚待实现 | [Components.h](../../../Engine/Scene/Components.h) 仅含 Identity、Transform、SpriteRenderer、Camera、LuaScript；[AssetMetadata.h](../../../Engine/Asset/AssetMetadata.h) 仅三类资源；无对应游戏系统模块 |
| v0.11 / Release | 尚无完整 Game 交付，也无查询到的 GitHub Release | 跟踪目录只有 Sandbox/SandboxProject 等，未有 Game；根 CMake 仍为 0.9.0；不把版本号当发布证明 |

不提供“v0.10 完成百分比”：八项能力复杂度不同，项目设置与输入完成不能推算剩余工作量。

### 与上一轮调研相比的变化

- 10-01 项目配置与 10-02 输入已由同一个 PR #82 交付；10-00 的 Stage A 设计也已有专项文档，不需重做。
- 原先“无 project.json、无鼠标事件、Lua 只能查 key”已不成立；这些条目只保留为 2026-09-06 历史结论。
- 10-03 共享调度仍未交付。Stage A 的两个入口共用配置/输入定义，并不意味着它们已经共用执行阶段。
- PM 后续方向没有改变；这次重点是把下一包做实，并补出 UI 实现所依赖的具体缺口。

## 3. PM 与架构文档对账

读取了 [PRD](../../Janus%20Engine%20产品需求文档（PRD）.md)、[技术架构](../../Janus%20Engine%20技术架构设计.md)、版本路线图、前次路线、Stage A 专项设计和验收记录。PRD §54 指定版本路线图是详细版本边界的唯一来源。

| 不一致或未决项 | 本方案处理 |
|---|---|
| Stage A 设计/路线仍写“尚未合并” | 同步合并事实，保留原实施基线和历史验收；新增本文作为接续入口 |
| PRD §4 将 UI/动画/音频/物理列 P0，路线图 §18 部分列 P1 | 保留产品必需性；施工顺序以版本路线图为准，不能因为 P1 字样删除 v0.10 承诺 |
| PRD §43 希望脚本故障后 Runtime 尽量继续；现有 Editor 采用 Faulted 保留现场 | 10-03 保留现有契约。脚本级隔离作为独立策略议题，不在共享执行阶段时暗改 |
| 架构 §6 画有 Fixed Update/Animation/Physics；当前只实现变量 Lua 更新 | 把图视为目标流程；10-03 只抽取实际运行阶段，物理阶段再确定固定 tick 与系统顺序，不宣称已有 Scheduler |
| PRD §4.4/架构 §2.5 的“所有修改进 Command”与项目设置独立保存有表述张力 | 沿用 Stage A 明确细化：Scene 作者态通过 Command；项目设置由 Engine 保存 API 与 Session guards 管理，独立于 Scene Undo/事务 |
| PRD 的 Headless、测试接口、完整资产流程和分级权限未被后续版本逐项覆盖 | 纳入 v1.0 前产品对账，不能因已有 MCP 和测试 fixture 宣称全部完成 |
| v0.11 路线建议联网卡牌 Demo，PRD 不要求完整 Multiplayer Engine | 先完成单机小样；保留 v0.11 的联网验证议题，立项时明确最小范围及是否为发布门槛 |

AGENTS 的“starting with Stage A”应在启动 10-03 实现时同步为新的活动工作包；Stage A 的兼容与输入约束继续有效。本次不把提案写成已启动实现。

## 4. 决定开发顺序的代码事实

| 发现 | 证据 | 下一阶段影响 |
|---|---|---|
| 两条运行路径各自驱动 ScriptEngine | [Application.cpp](../../../Engine/Application/Application.cpp) 的 managed 分支负责 Create/Start/Reload/Update/Stop；[RuntimeSession.cpp](../../../Engine/Runtime/RuntimeSession.cpp) 另有 Start/Advance/Step | 必须共享实际执行与清理阶段，避免新系统分叉；宿主错误呈现不必相同 |
| 两条路径生命周期不同 | Application 对其加载/创建的 Scene 运行，磁盘项目 Lua 错误返回失败并清理；RuntimeSession Clone 并保留 Faulted 世界 | 不能简单让 Application 调用会 Clone 的 Start，导致客户端仍引用旧 Scene；需明确 Scene 所有权与引用稳定性 |
| UI 绘制顺序不能直接沿用现有分批排序 | [RenderQueue.cpp](../../../Engine/Renderer/RenderQueue.cpp) 按 layer、texture、blendMode 排序 | 推断风险：同层半透明 UI 可能改变遮挡顺序。先测试 A/B/A 纹理的重叠元素，再设计保持 UI 顺序的提交路径；这不是本次已复现的现有缺陷 |
| 文本和裁剪需要实际能力 | Asset 无 Font 类型；[RenderDevice.h](../../../Engine/Renderer/RenderDevice.h) 无现成裁剪接口 | 不能把 Text 当 Sprite 改个名字。确定字形资源、度量、换行和矩形裁剪的范围 |
| 输入有路由但还没有 Game UI 消费阶段 | [Stage A 设计](../specs/2026-09-06-v0.10-project-input-design.md) 将 UI/世界消费留给后续 | 同一次点击必须有唯一目标；Game View 隔离只解决 Editor 面板与游戏的边界 |
| 层级底层已有，作者态命令不完整 | Scene 支持 SetParent，EntityCommands/EditorActions/MCP 没有独立 Reparent/Duplicate 操作 | 在 UI 树制作需要时补 Reparent 的同源入口；不能在 Hierarchy panel 直接改 ECS |
| 通用反射还没有实体引用值类型 | [ReflectionTypes.h](../../../Engine/Core/Reflection/ReflectionTypes.h) 有 AssetReference，没有 EntityReference/任意容器 | Button 首版可约束为当前实体的脚本回调，避免泛化事件绑定器；Prefab 内部实体引用需要专门设计，不能把 UUID 字符串都当实体引用重映射 |
| 可读 runtime/entity 不等于可读任意游戏状态 | [DebugResources.cpp](../../../MCP/Resources/DebugResources.cpp) 暴露反射实体快照；ScriptEngine 未提供任意 Lua table 的观察入口 | Game 需显式导出有限状态，让“伤害是否生效”可以验证，不往 Engine 加 Health/Card |

## 5. 下一阶段可执行工作包

保持前次编号，按顺序交付。复杂度 S/M/L 仅表示相对不确定性，不对应日历工期；10-03 为 M，UI 与文本为 L，交互/小样为 L。记录实际周期后再估算后续。

### 10-03：共享 Runtime 执行阶段（第一个开发任务）

**结果：同一场景、相同输入与时间步，在两个入口得到相同 gameplay 状态；现有暂停、单步、错误和清理契约保持。**

设计方向：在 `Engine/Runtime` 内抽取被两个宿主复用的具体执行对象，名称在专项设计中确定。对象管理 ScriptEngine 及其稳定输入缓冲，引用显式传入的 Scene/AssetService；不负责窗口、ImGui、MCP 或 CommandBus。Scene 的所有权仍属于宿主：Editor 管 Clone，Application 管独立世界。ScriptEngine 必须先于 Scene/AssetService 销毁。

本包固定的顺序为：宿主完成输入准备 → 允许时热重载 → ScriptEngine Update → 返回结果 → 宿主渲染/呈现。保留 `ApplicationClient::OnUpdate` 当前调用时机。未来 UI/物理接入时再扩展这个唯一阶段入口，不提前建立动态注册、任务图或空系统集合。

先写测试，再迁移实现：

1. 从同一磁盘 fixture 启动两种入口，注入相同 bindings、输入帧与 dt，逐帧比较可观察 Transform 与回调次数，验证脚本更新不重复。
2. 保留 Application 初始化/更新/退出顺序、错误 Result 和资源清理次序；验证初始化中途失败和重复停止。
3. Playing 正常刷新脚本；Paused 不推进；Step 只推进一次 1/60 秒、输入中性、不 reload；Stop/重开读取新源文件。
4. Lua 故障后 Editor 保留 Faulted Scene 和诊断，阻止推进及 authoring；独立入口仍按既有契约返回失败并退出。相同故障发生在相同执行阶段，不强行统一宿主 UI/退出行为。
5. 保留 Runtime ID、frameIndex、simulationTime、partialUpdate 等现有语义，保留作者态 dirty/history、事务保护和 Scene v1 名称。

合并门槛：相关 Application/RuntimeSession 测试先通过，再跑两个 configure/build 与全量 CTest、modern/legacy 外部 stdio；记录两个入口各一次真实启动/正常退出。无需新依赖，不改生产 MCP 入口，不把测试 host 升格为产品。

### 10-04：可保存、可渲染的 UI 与文本

**结果：Human 和 Agent 都能构造一个包含 Panel/Image/Text 的菜单，保存重开后在两个入口显示一致。**

拆成两个连续 PR，每个都保持可运行的可见结果：

| 子包 | 内容 | 完成证明 |
|---|---|---|
| 10-04a | Canvas、矩形布局/Anchor/Pivot、Panel/Image、稳定绘制与命中顺序；最小 Reparent Command 及 Human/MCP 接入 | 修改 UI 属性 → Undo/Redo → 保存重开 → Play；叠放 A/B/A 三种纹理仍保持指定顺序；父级移动、尺寸变化、裁剪边界正确 |
| 10-04b | Font 资源/字形度量、Text、必要 Lua 文本更新入口、按名称/类型发现资产的只读 Engine/MCP 能力 | 菜单和数字血量能渲染与更新；缺字体/缺字有明确行为；资产搜索结果稳定、有界；FakeRenderDevice 自动测试及真实画面验收 |

首版布局建议：单一屏幕空间 Canvas，使用 project.json 的逻辑分辨率；轴对齐矩形、明确 sibling order、Anchor/Pivot/offset/size，足够支撑固定手牌排布。暂不承诺旋转 UI、世界空间 Canvas、复杂自动布局、滚动列表或任意形状 Mask。

字体建议：先用**离线字形图集 + 度量元数据**作为 Font 资源，复用现有纹理加载。支持有效 UTF-8 解码、已收录字形、换行和矩形裁剪；第一份资源覆盖小样实际文案、数字与占位符，不宣称全中文/复杂文字排版。字体来源、使用授权和图集生成方法必须可复现。若需要动态字体栅格化，再单独比较方案并记录依赖的精确 pin、边界及测试影响；本文不引入新依赖。

UI 顺序与裁剪由专项设计明确：可以扩展顺序保持的 Renderer 提交方式，但不得破坏现有世界 Sprite 批处理。只批合并不改变遮挡顺序的相邻元素。首版轴对齐矩形可评估 CPU 裁切顶点/UV；如果改为后端裁剪，必须经 RenderDevice 封装，不允许 UI 出现 gl*。图形渲染序与反向命中序共用一个已计算结果。

所有新增 authoring 数据必须在本包贯通 Reflection、Scene persistence/Clone、Inspector、Command 和 MCP schema。Reparent 明确保持局部布局数值，恢复原 parent/sibling order；拒绝 cycle、无效 UUID 和不受支持的跨 Canvas 操作。布局缓存、hover、focus 不写入 Scene。

### 10-05：交互、单局规则与可读游戏状态

**结果：鼠标与键盘均可完成固定战斗，显示真实数值；Agent 能验证“出牌后掉血”。**

仍拆两个连续 PR：

| 子包 | 内容 | 完成证明 |
|---|---|---|
| 10-05a | Button、焦点遍历、Confirm、Lua 回调、UI/世界输入消费；以点击计数菜单作为可运行切片 | 重叠仅最上层响应；一次点击一次回调；禁用/隐藏/失焦不激活；键盘选择与 Confirm 可完成同一动作 |
| 10-05b | `Game/` 固定战斗、菜单/胜负/重开、显式游戏诊断快照与 Agent 验证样例 | Human 双入口完成一局；自动序列的规则结果一致；MCP 读回伤害、血量和阶段；Stop 后作者态原值不变 |

输入规则必须在实现前确定：UI 先获取该帧 Game View 输入，对合法 press 捕获目标；在同一有效目标释放才 Click，拖出/失焦取消。短按、隐藏/删除捕获对象、按住后跨 UI 边界要有测试。消费产生的 gameplay 输入屏蔽相应按键/指针边缘与 held 状态，保持释放状态连续，避免世界重复响应或卡住。Canvas 没有消费时，旧 key/action 查询继续保持 Stage A 行为。Core 不依赖 UI，消费发生在 Engine Runtime/UI 边界；事件回调统一在宿主主线程执行。

首版 Button 绑定当前实体的受控 Lua 回调，不解释任意表达式；先收集 UI 事件、校验持久 UUID 和存活性，再派发，防止回调销毁实体造成悬空引用。UI 事件、Lua 更新和必要的布局刷新顺序写入测试；不要在 Editor 单独复制一套。

建议固定验收规则由 Game/Lua 拥有：敌方初始 HP=12；普通攻击造成 4 点伤害，三次有效攻击进入胜利；另有确定的失败路径；重开恢复所有局内数值。该数值只是测试 fixture，Engine 不定义 Card、Health 或回合规则。

游戏诊断建议为脚本显式发布的有界只读快照：字段名由 Game 定义，例如 `phase`、`enemyHp`、`lastDamage`、`turn`；Engine 只支持有限标量、运行 ID/帧索引、大小限制和生命周期清理。MCP 读取主线程上已发布的数据，不在 read handler 执行 Lua，不提供任意 table 遍历或 eval。独立入口和 Editor 使用同一数据模型，未知字段、停止后读取和旧 Runtime ID 明确报错或返回未运行状态。

Agent 验证路径需要区分：

- Human 真实点击及键盘游玩验证完整 UI；确定性测试通过宿主注入输入验证双入口一致。
- MCP 当前 `runtime.step` 必须继续保持中性输入，不能拿它冒充点击或改成输入注入工具。Agent 回归可打开一份 Game 自有验证场景，由 fixture 在 Update 中调用与 Button 相同的战斗规则，随后通过 Step 和快照断言结果；这验证规则与观测链，不能替代真实 UI 命中测试。
- Agent 修改已有 UI/Scene 参数仍通过已反射的 authoring 属性；需要游戏状态诊断时使用受控新资源。Game/Lua 内的战斗常量尚无 MCP 修改接口，修复这类脚本问题需使用项目代码编辑工具并按现有 reload/重启语义验证，不能声称已有 `Health` 属性写工具。尚未实现生产测试工具的事实继续在产品对账中记录。

10-05 出口包括：中文或拉丁文测试文案显示符合声明字符集；非 16:9 窗口/letterbox、Game View 隐藏、文本面板焦点不触发游戏；同一固定输入序列在两个入口得到相同结果；保存重开、Undo/Redo、Faulted/Stop、modern/legacy MCP 均回归通过。

## 6. v0.10 剩余主线与交接门槛

```text
已完成：10-01 配置 + 10-02 输入
    → 10-03 共享 Runtime
    → 10-04 UI 显示/文本
    → 10-05 交互/单局小样/观测
    → 10-06 AnimationClip + Animator
    → 10-07 Audio
    → 10-08 Physics
    → 10-09 Prefab
    → 10-10 综合验收
    → v0.11 Production Demo
```

| 工作包 | 产品交付 | 必要验收与范围限制 |
|---|---|---|
| 10-06 | 出牌/角色 Sprite 动画；帧、时长、Loop、Play/Stop/Switch | 资源 Handle、Runtime cursor 与 authoring 分离；Pause/Step/Stop、缺资源与销毁可测试，不做状态机编辑器 |
| 10-07 | 一次出牌音效 + 循环背景音 | Clip/Source、音量与播放控制，设备所有权/释放顺序明确；fake/null 测试和真实设备验收；无设备可诊断，不阻止项目打开 |
| 10-08 | Box2D 最小封装 + 独立物理验收场景 | RigidBody2D/Collider/Trigger/Raycast/回调；固定 1/60 tick、有限补步、每帧输入边缘只消费一次；固定步骤与 Lua/动画顺序专项设计，Step 不改变语义 |
| 10-09 | 可持久化的实体子树模板和可撤销实例化 | 新 UUID、内部实体引用重映射、外部 AssetHandle 不变、失败不遗留半树、一次 Undo；没有实例 override/自动传播承诺 |
| 10-10 | 小样 + 物理场景覆盖八项能力，形成版本证据 | 新组件 Reflection/持久化/Editor/MCP 全贯通；真实 Agent 任务与 CPU/渲染基线；Demo、Tests、Documentation 同时通过后才标记 v0.10 完成 |

物理对卡牌玩法不是前置依赖，放后面是交付价值排序；Prefab 后置是先验证真实复用需求。若 10-04/05 确需重复对象，可补最小 Duplicate，同源 Command/UUID/Undo 一并交付；这不等于 Prefab 已完成。

Prefab 在实例化前必须补齐实际用到的实体引用语义。当前反射尚无 EntityReference；若 UI 只用 Hierarchy 与同实体回调，则先不增加。不能给未实现的引用种类写一个名义上的“全部重映射”验收。

## 7. 资源投入、风险与延期策略

首个实施轮只承诺 10-03；通过后开始 10-04a。每个 PR 包含可运行行为、相关测试和文档，不同时铺开动画/音频/物理。记录每包实际开发、验证和返工时间，用 10-03/10-04a 的结果更新估算。

| 风险 | 触发信号 | 处理 |
|---|---|---|
| Runtime 抽取变成框架重写 | 出现大量未使用的 System 接口或强制统一宿主状态机 | 收回到共享现有执行对象；保留宿主 Scene 所有权与错误契约 |
| UI 范围膨胀 | 字体回退、自动布局、拖拽编辑器开始阻塞菜单 | 首版固定字形集/轴对齐布局/Inspector 制作；复杂能力另排，不删除 Text/Button |
| UI 绘制与命中不一致 | 重叠测试中看到的顶层与回调对象不同 | 固定共同顺序数据；先保正确性，再测 Draw Calls 优化 |
| Agent 无法确认结果 | 只能读日志字符串或看到 Lua 源码 | 10-05b 必交结构化快照与确定性场景，禁止用“已接 MCP”替代结果可读 |
| 设备测试被假设备测试代替 | 测试全绿但未在原生窗口/音频设备运行 | 自动回归与真实设备证据分栏，未执行项显式保留 |
| 版本范围被悄悄缩减 | UI 小样完成就宣布 v0.10 完成 | 10-10 逐项对齐八项能力；物理验证场景、Prefab 不能遗漏 |

新增第三方依赖在对应工作包立项时给出用途、精确 tag/commit、许可证/资源授权、PUBLIC/PRIVATE 封装、Windows/MSVC/CMake 支持与 fake 测试策略。10-03 预期零新依赖；UI 字体优先离线资产方案；Box2D 是架构指定路线，音频库仍需具体选型。本文未更改项目许可证。

## 8. v0.11 / v1.0 产品对账清单

| 节点 | 下一次需要形成的明确决定 |
|---|---|
| v0.11 立项 | 完整单机 Roguelike 的内容规模、存档、多 Scene、独立运行交付；最小联网验证和发布门槛，Game 代码独立于 Engine |
| v0.11 Benchmark | 路线图 Task A–D：创建 Battle 场景、配置角色 Sprite、定位 Health 问题、分析 Draw Calls；固定 fixture、模型/工具版本、尝试次数、成功率、人工介入与失败类型 |
| v1.0 Human 闭环 | 创建项目/Scene 切换、Import/Reimport/Rename/Move、资源引用与错误恢复、构建和分发；逐项标记必交或明确延期 |
| v1.0 Agent 闭环 | 项目设置等核心 Human 操作的 Agent 等价能力；正式 List/Run Tests、生产 Headless、分级权限/Dry Run、N 帧采样的版本归属 |

这些是 PRD 对账项，不因为本次列出就自动扩大 10-03～10-05 范围。Release 发布和版本号调整在版本验收时单独处理。

## 9. 本次调研验证记录

通过 Visual Studio 2022 Developer PowerShell 执行；本地跟踪源码与上述 main tree 相同。读取了完整 configure/build/CTest 日志。

| 命令 | 本次结果 |
|---|---|
| `git fetch origin main`、`git rev-parse HEAD origin/main`、tree 比较 | 已更新 main；HEAD 与 main 提交不同、源码 tree 相同，未切换分支 |
| `gh pr view 82 --json number,title,state,mergedAt,mergeCommit,statusCheckRollup,url` | MERGED，PR Windows 检查成功 |
| `cmake --preset windows-msvc-debug` | 成功 |
| `cmake --build --preset windows-msvc-debug --parallel 2` | 成功，增量构建 `ninja: no work to do` |
| `cmake --preset windows-msvc-debug-tests` | 成功 |
| `cmake --build --preset windows-msvc-debug-tests --parallel 2` | 成功，增量构建 `ninja: no work to do` |
| `ctest --preset windows-msvc-debug-tests` | **299/299 passed，0 failures，9.85 秒**，包含 modern/legacy 外部进程 stdio |
| `gh release list --limit 5` | 无返回条目 |
| `gh run view 34045665651 --json status,conclusion,url` | main `c8a7c06` 的 Windows CI 为 completed / success |
| `git ls-remote origin refs/heads/main` | 交付前确认远端 main 仍为 `c8a7c06` |
| 修改文档本地链接检查 | 38 个链接，0 失效 |
| `git diff --check` | 通过；新文档另做空白检查 |
| `git check-ignore out/20260907-roadmap-ctest.log` | 生成日志被忽略 |

远端 CI、本次本地自动复测与历史真实 Editor 验收分别记录，不互相替代。

本次日志位于 ignored `out/20260907-roadmap-*.log`。配置报告可选 PkgConfig/LibUSB 缺失，但两套配置成功。本次为增量构建和自动复测，不是干净重编译；未重新验证原生图形输入、音频设备或新增游戏系统，也没有提交、推送或发布。
