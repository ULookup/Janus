# 首个 Release 编辑器优化实施计划

日期：2026-09-07；基线 `7ecdfb8`。初版为建议顺序，无工期承诺。
后续用户明确要求按参考图升级 UI，已先实现 R2–R5、R7 中的布局、资源槽、取景与诊断展示部分。
详见 [UI 升级验收](../../verification/2026-09-07-editor-ui-upgrade.md)。R1 未完成，
本轮不满足检查点 A，也不代表 R2–R7 整体完成；下表仍是完整发布准备的待办框架。
产品目标及交互以[体验设计](../specs/2026-09-07-first-release-editor-ux-design.md)为准。
本计划不将首个 v0.10 Preview 自动改称 v1.0，也不启动 v0.11 游戏系统。

## 实施顺序

先修数据保护，再建立统一工作区与交互规则，然后逐条补制作闭环，最后用 Release 包复验。
每个切片独立可测试、可合并；前一项出口满足后再进入依赖项，不同时铺开多个未闭环子系统。

| 顺序 / 工作包 | 范围与主要代码入口 | 依赖 | 交付验收 |
| --- | --- | --- | --- |
| R1 安全退出 | Application / Window 关闭请求协商、Editor dirty 对话框、保存失败保留会话；覆盖事务/Runtime/recovery 退出策略 | 先写边界设计和失败测试 | X/Alt+F4/菜单三入口的保存/放弃/取消、错误重试；既有 Application 无交互退出与 MCP teardown 不回归 |
| R2 工作区与可读性 | EditorWorkspaceLayout、EditorApplication；可调 split、稳定工具栏、Inspector 全高、底部折叠、三布局；本地配置、核心中英标签、DPI/font fallback | R1 | 各目标尺寸/缩放可用，布局重开与损坏回退，Runtime 控制不位移；项目设置移入独立窗口 |
| R3 选择、输入与属性基础 | EditorContext、HierarchyPanel、InspectorPanel；选择联动、搜索、单一 Add Component、字段单位与校验、焦点优先级 | R2 | 输入框 Ctrl+Z/Delete 不触发场景操作，组件增删/属性修改走共享命令；Runtime 只读标识准确 |
| R4 资源赋值与 Prefab | AssetBrowserPanel、InspectorPanel、EditorActions、ProjectSession::ExportPrefab；统一资源槽、类型筛选/搜索/目录、固定操作栏、可读导出名 | R3 | 六类组件资源槽全覆盖，nil/错误类型/缺失资源；Prefab 导出补偿、Undo/Redo 与保存重开通过 |
| R5 场景操作与取景 | EditorCamera、ScenePicker、EditorActions；Fit/Focus、选择框、单对象 Move、Duplicate，受保护的临时预览 | R3；资源用例依赖 R4 | 拖一次一个 Undo，Esc/失焦无改动；父子变换与 UIRect 边界清晰；UUID/primary camera/Canvas 约束继续有效 |
| R6 文件生命周期 | ProjectSession、启动入口、Scene 序列化复用；欢迎页/打开项目/最近项目、单活动 Scene 新建/打开/另存；本地示例副本 | R1，R3–R5 作为闭环样本 | 打开失败保留旧项目；保存失败不换路径；切 Scene 刷新 MCP 绑定/选择/历史；默认场景配置不被暗中修改 |
| R7 诊断与示例收口 | Console/Profiler/Agent Activity、运行提示、Game/Combat；状态显示、错误定位、按钮阶段可用性 | R2–R6 | 重走胜负、重开、暂停/单步、热重载/Faulted 路径；CPU 与 GPU 不混标 |
| R8 Release 候选包 | CMakePresets、CI、版本号/notes、资源/字体/依赖许可、解压包与入门说明 | R1–R7 | Debug 全量 + 新增 Release 配置验证 + 干净机器 + PM 任务验收全部通过，才创建 Tag/Release |

R6 涉及现有 API 缺口，不得用修改默认场景 JSON 后重启来伪装普通 Open Scene。
复制使用现有 Scene 子树快照和新 UUID，不能复制源 UUID；非法多主相机/Canvas 等明确拒绝。
导出可读名称需扩展受保护导出入口，不在 UI 层自行改文件或绕过 registry 原子保存。

## 技术验证要求

1. R1、R5、R6 先提交简短设计及失败用例，确认生命周期/预览边界后实现。
2. 每个行为切片先补有意义的自动测试，再实现；运行最窄相关用例后跑完整测试预设。
3. 所有切片保留 Human/MCP 共享 guards，尤其 Runtime/事务/补偿失败；新 UI 禁用不能代替后端校验。
4. R2 本地偏好损坏、超范围尺寸、缩放坐标、无中文字体 fallback；R3–R4 引用类型拒绝、Undo/Redo、清空、保存重开；
   R5 指针丢捕获、Esc、非法父变换；R6 文件失败、旧会话保留、Scene revision 等作为重点回归。
5. 原生 UI 验证在项目副本完成，截图/日志记录窗口尺寸、系统缩放、构建 SHA 与操作步骤。
6. Release 增加真实 preset 后再固定命令；不引用一个尚不存在的 Release preset。

已有 Debug 验证命令（从 Visual Studio Developer PowerShell 执行）：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
git diff --check
```

当前 412/412 是 `7ecdfb8` 的历史基线，不是 R1–R8 已完成的证据。
不能为了“保持 412”拒绝合理新增测试，也不能用测试数量推导完成百分比。

## 产品检查点

| 检查点 | 可演示结果 | 进入下一阶段的条件 |
| --- | --- | --- |
| A：R1–R2 | 未保存关闭可取消，布局可调，文字清楚 | 数据保护回归通过；目标机器无关键栏目遮挡 |
| B：R3–R5 | 找 Hero → 改属性 → 选动画 → 移动 → 导出/实例化 → Undo | 无 JSON 编辑的已有内容制作任务成功；无错误对象或 dirty/history 副作用 |
| C：R6–R7 | 新建/打开场景 → 保存/另存 → 运行/错误定位 → 重开 | 普通用户能自己完成一次场景迭代；所有 PM-01～PM-06 有关闭证据 |
| D：R8 | 解压安装包，用随包材料完成验收任务 | 自动测试、实机任务、版本/分发检查完整；无未关闭阻断问题 |

5 人试用是小样本可用性验收，不代表统计意义上的大规模成功率。每次保留实际耗时与求助记录，
未达到设计目标则定位具体流程调整，再复验相关任务。不得用内部开发者记忆操作替代首次用户测试。

## 范围调整原则

工期尚未估算：R1/R5/R6 的边界复杂度需先技术拆解；本计划不凭 UI 面板数量承诺完成日期。
如需缩减首版，优先去掉自由 docking、多窗口、批量操作等已排除功能；不能删掉数据保护、
可读性、Image/Animator 赋值或保存重开来换取外观进度。
新建项目向导/通用导入不在该 Preview 优化包内，须在发布说明中明确当前支持的已有项目与资源准备流程。
若对外定位变为“完整创作工具”，则必须另补这些 PRD 工作后再宣布达成，不能沿用 Preview 出口。

初版设计阶段交付为设计文档与布局示意；随后 UI 实现的最新状态见本文开头及专项验收。

设计交付检查：文档本地链接及代码围栏检查通过，`git diff --check` 通过；
交互示意 `node --check` 通过，浏览器验证了 dirty 提示、关闭取消、动画资源选择、
运行只读、布局切换和停止恢复原视图，并检查桌面及窄宽显示。
这些仅验证方案呈现，不能计为原生 Editor 的 R1–R8 完成证据。
