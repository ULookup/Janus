# v0.10 原生编辑器 PM 验收

日期：2026-09-07，北京时间。代码基线：main `7ecdfb8`。本轮从实际用户操作出发，
以真实 JanusEditor 窗口验收综合示例及基础制作流程，再用源码与落盘文件核对观察。

## 验收结论

**综合示例演示通过；编辑器日常制作及发布验收暂不通过。**
胜负、重开、暂停隔离、基础作者态命令、Prefab 导出/实例化及保存重开均可复现。
发现一个高优先级数据保护问题：带未保存改动关闭窗口时，直接退出且改动丢失。
另有 Human 资源赋值缺口与可用性问题，详见下表。此结论不否定已有系统集成测试，
也不代表已验收全部编辑器功能、音质、跨设备显示或 Release 性能。

## 环境、隔离与证据

- 执行文件：`out/build/windows-msvc-debug/Editor/JanusEditor.exe`，本日 14:47:41 构建。
  SHA256：`B00EF43CFF3D9CAFCC7FA7F4484ECE03CAFD3FDC3C0F5CBF8842ED1FF33AA6DB`。
- 启动前重新核对 `git ls-remote origin refs/heads/main`，仍为 `7ecdfb8`；
  当前工作分支 `codex/v0.10-project-status` 仅有文档改动，没有更新实现。
- 将 `Game/` 完整复制到 `out/pm-editor-acceptance-20260907-150723/Game/`，
  使用 `JanusEditor.exe --project <副本路径>` 启动。所有制作和保存操作仅作用于副本。
- 首次进程于 15:07:24 初始化，15:16:04 正常退出；15:16:25 重开成功。
  日志报告初始窗口 1440×900；UI 工具截图为约 963×631，最大化为 1707×1019。
  不将截图尺寸直接当作面板物理像素，也未测量或修改系统 DPI 设置。
- 结束时原始 `Game/` 的 28 个文件 SHA256 与启动前一致。副本保留一个已保存的
  `PM_Acceptance_Probe` 实体及导出的 Prefab，编辑器保持打开、Stopped、Game 预览。
- 16 张原生截图、两轮 stdout/stderr 与 `source-hashes.json` 保留在上述 `out/` 目录。
  它们已被 Git 忽略，不随文档提交；下述图片链接仅在本验收工作区可用。

## 实际执行结果

| 操作 | 观察及结果 | 证据 |
| --- | --- | --- |
| 打开默认项目副本 | Integrated，18 实体 / 16 资源，Stopped；Scene 初始取景可用性欠佳，切 Game 后内容完整 | [初始 Scene](../../out/pm-editor-acceptance-20260907-150723/01-initial-scene.png)、[Game 预览](../../out/pm-editor-acceptance-20260907-150723/02-game-preview.png) |
| Play 与鼠标战斗 | 点击 Start → Strike → Play；敌方 12→8，我方 6→4，Turn 1；机器人落至地面，受击/按钮动画有可见变化 | [第一击](../../out/pm-editor-acceptance-20260907-150723/03-first-strike.png) |
| 数字键战斗与胜利 | 数字键 2 选 Strike、4 出牌，补完两回合；敌方 8→4→0，我方 4→2→2，Turn 3 / VICTORY | [胜利](../../out/pm-editor-acceptance-20260907-150723/04-victory.png) |
| Pause / Step / Resume | 暂停后点击 Restart 无效；Step 后仍 Paused、胜利状态未变；Resume 后未重放暂停期间点击 | [暂停单步](../../out/pm-editor-acceptance-20260907-150723/05-paused-step.png) |
| 重开与失败 | Playing 时鼠标 Restart 回初始菜单；数字键 1 开局、3 选 Wait、4 出牌，三轮后敌方 12，我方 0，Turn 3 / DEFEAT | [失败](../../out/pm-editor-acceptance-20260907-150723/06-defeat.png) |
| 运行隔离与 Stop | Playing/Paused 时作者态操作禁用；Stop 返回 Scene，手动切 Game 后 HP、回合、角色位置回到编辑状态 | [停止还原](../../out/pm-editor-acceptance-20260907-150723/08-stop-restored.png) |
| 创建与重命名 | +Entity 后 18→19、出现 Unsaved；Inspector 输入 `PM_Acceptance_Probe`，Enter 后层级同步 | [重命名与重做](../../out/pm-editor-acceptance-20260907-150723/09-rename-redo.png) |
| Undo / Redo | Undo 将名称恢复 Entity；Redo 恢复验收名称 | 同上 |
| Prefab 导出/实例化/Undo | Export Prefab 后资源 16→17；资源筛选 Prefab，选中新资源并滚动至 Instantiate；实体 19→20；Undo 回 19 | [Prefab 实例](../../out/pm-editor-acceptance-20260907-150723/11-prefab-instantiated.png) |
| 保存与重开 | Save 后 Unsaved 消失；关闭后重开，已保存的 Probe 保留，场景 JSON 同样为 19 个实体 | [已保存](../../out/pm-editor-acceptance-20260907-150723/12-saved.png)、[重开](../../out/pm-editor-acceptance-20260907-150723/14-reopened-saved-only.png) |
| 未保存改动关闭 | 保存后再 +Entity，20 实体 / Unsaved；点击窗口 X，没有确认对话框，窗口直接消失；重开恢复 19，未保存 Entity 丢失 | [关闭前](../../out/pm-editor-acceptance-20260907-150723/13-unsaved-before-close.png)、重开图 |
| Profiler / Console | CPU 帧和 Runtime 阶段数据可见、更新；面板明确声明不是 GPU 时间；日志未见 error，存在 OpenGL shader 重编译性能 warning | [Profiler](../../out/pm-editor-acceptance-20260907-150723/07-profiler.png)、[最终 Console](../../out/pm-editor-acceptance-20260907-150723/16-final-editor-console.png) |
| Human 动画资源入口 | Hero 的 Animator.clip 仅显示 UUID；Asset Browser 类型菜单没有 AnimationClip；结合源码确认缺赋值入口 | [类型筛选](../../out/pm-editor-acceptance-20260907-150723/10-asset-types.png)、[Animator](../../out/pm-editor-acceptance-20260907-150723/15-animator-uuid-only.png) |

这些操作验证的是已列出的路径。没有通过“灰色按钮”推断底层权限全覆盖；底层 guards 的
依据仍是既有自动测试。Prefab 本轮使用只有 Transform 的临时根实体，未重演复杂子树全套回归。

## 问题与后续验收标准

优先级为本轮 PM 建议，未自动创建 Issue 或修改版本范围。P1 为发布前阻断问题，
P2 为当前制作体验或入口缺口，P3 为演示交互完善。实现修复需另行补测试与回归。

| ID / 优先级 | 问题、影响与证据 | 建议修复后的验收标准 |
| --- | --- | --- |
| PM-01 / P1 | **关闭未保存场景直接丢改动。** 按上表 Save → +Entity → X → 重开复现一次；未见保存/放弃/取消提示，正常退出日志排除本次为崩溃。源码中 [Application](../../Engine/Application/Application.cpp) 在 WindowCloseEvent 直接 RequestExit，SDLWindow 先设置 ShouldClose；[Editor 关闭](../../Editor/EditorApplication.cpp) 释放 ProjectSession，未执行 dirty 确认。置信度高 | dirty 场景关闭提供 Save / Discard / Cancel；Cancel 保留现场，Save 成功才退出，保存失败保留窗口与改动；干净场景可直接退出。窗口 X、Alt+F4 及其他退出入口分别回归。不能只在退出已提交后追加对话框 |
| PM-02 / P2 | **首次 Scene 视图难以找到综合场景。** 默认与最大化均几乎只有白色网格和中心小方块；选中 Hero 后取景仍未定位角色，切 Game 能看到完整场景。属于作者态导航/默认取景问题，不据此判定 Game 渲染失败。置信度高 | 打开默认示例即可看到有效场景范围，或提供清晰的 Frame All / Frame Selection 与使用提示；选中角色可一步定位 |
| PM-03 / P2 | **编辑器文本及操作目标偏小。** 当前显示环境下工具栏、层级和属性文字明显小于系统标题及 Game 内容，最大化没有改善；[字体配置](../../Editor/EditorApplication.cpp) 使用固定 16 字体。现象置信度高，DPI 根因尚未验证 | 在目标设备及常见缩放设置实测可读性，提供有效 UI 缩放或正确 DPI 适配，保证文字与点击目标同步缩放 |
| PM-04 / P2 | **面板尺寸固定，资源操作被挤出首屏。** 最大化下 utility 面板仍窄，Prefab 的 Instantiate 必须滚动；尝试拖边界不能扩展。源码 [WorkspaceLayout](../../Editor/EditorWorkspaceLayout.cpp) 将 utility 高度上限设为 230 个布局单位，[Editor](../../Editor/EditorApplication.cpp) 使用 NoResize / NoSavedSettings。置信度高 | utility/Inspector 可调整大小，常用资源动作易发现；切换面板时保持清晰导航，窗口恢复时可保留用户布局 |
| PM-05 / P2 | **Human 无法完整配置现有资源组件。** 实机看到 Animator.clip 只读 UUID、筛选缺 AnimationClip；[Inspector](../../Editor/Panels/InspectorPanel.cpp) 的 AssetReference 仅显示文本，[Asset Browser](../../Editor/Panels/AssetBrowserPanel.cpp) 没有 Animator.clip / Image.texture 赋值分支。与前轮 PM 对账一致，置信度高 | 从 Editor 选择注册动画资源并赋给 Animator、纹理赋给 Image，支持 Undo/Redo、保存重开和 Runtime/事务 guards；若本版保留限制，发布说明必须给出明确替代步骤 |
| PM-06 / P2 | **导出的 Prefab 不易识别。** 临时实体导出后显示为 `Prefabs/<UUID>.prefab`，没有名称/路径选择；多次导出将难以区分内容。本轮仅导出一次，置信度高 | 保持稳定身份同时展示可读名称，导出成功能定位资源并说明结果；不要求改变现有 UUID 身份契约 |
| PM-07 / P3 | **示例按钮缺少阶段可用性提示。** 胜利/失败后 Start、卡牌、Play 仍保持与有效动作相近的视觉样式；本轮未穷举每个无效按钮的逻辑后果，不能认定其会错误扣血 | 按当前菜单/战斗/结算阶段和选择状态呈现可操作项，禁用或解释无效动作，胜负后清楚引导 Restart |

PM-01 是本轮新增的实际数据丢失路径；PM-05 为既有代码核对结论的原生 UI 佐证。
其他问题大多影响发现性和制作效率，不应与系统未实现或运行崩溃混为一谈。

## 验证边界与收尾

本轮没有改 C++、Lua、正式场景或项目配置，因此复用同代码基线刚完成的
[configure/build/CTest 记录](2026-09-07-project-status-audit.md)：两套 Debug 预设成功，
412/412 测试通过，32.90 秒。没有再次运行 CTest 来把 UI 缺陷写成“自动测试通过”。
实际执行包括原生窗口鼠标/数字键操作、`git ls-remote`、日志全文读取、Scene JSON 读取、
`Get-FileHash` 核对及文档 `git diff --check`。

未覆盖：真实听感/音量与设备切换、所有键盘焦点及拖拽取消边界、热重载、故障恢复、
MCP/事务复验、复杂 Prefab、通用资产导入、Transform 数值修改、跨项目/多场景流程、
Release/GPU/Present 性能。未新增发布声明或修改版本号。
OpenGL 日志 warning 是待后续性能核对的观察，不据此认定性能不达标。

发布建议：先修 PM-01 并完成退出/保存失败回归；明确 PM-05 的版本处理，
处理首次使用和目标设备可读性，再开展 Release 与分发验收。编辑器当前保留在副本
Game 预览、Stopped 状态，正式示例未被本次验收改写。
