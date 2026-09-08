# D1：本地偏好、双语与 Inspector 草稿验证

日期：2026-09-08。分支 `codex/editor-preferences`；HEAD 基线
`59387eb0a03507207a9808610e9e8cfd8da152a8`，本轮实现位于未提交工作区。
保留 C 包全部改动；此记录的结果覆盖 A/B/C 集成后的 D1，不能套用为 main 或 PR CI。
#95 已合入 main、#96 合入 A 分支的历史关系见 [C 包记录](2026-09-08-editor-move-gizmo.md)。
D2 日志详情/运行故障入口/示例按钮状态尚未开始，v0.10 未发布。

## 实现边界

- `EditorPreferences` 保存用户缩放、语言、Standard/Focus/Debug 布局、分隔宽度、网格、
  组件展开状态、最近 10 个项目和最多 32 个项目/Scene 相机记录。
- 默认路径由 Platform 的 `SDL_GetPrefPath("Janus", "JanusEditor")` 提供；本机为
  `%APPDATA%/Janus/JanusEditor/editor-preferences.json`。不写 project.json 或 Scene；
  `JANUS_EDITOR_PREFERENCES_PATH` 可用于隔离测试。最近项目只记录数据，欢迎页入口属于 E2。
- JSON v1 上限 64 KiB、预解析嵌套上限 16、路径/列表/字段类型严格校验。
  损坏、截断、未知版本或超限会提示并回退；有限但越界的数值夹取并提示。
  500 ms debounce、拖动/输入期间延后写入、退出 flush；原子替换失败保留旧文件，
  不阻断 Scene 保存与退出，View 提供重试入口。
- 偏好项目键一次解析为绝对规范路径，避免不同工作目录的同名相对项目串档。
  相机是编辑器用户视图，不修改 Scene Camera。布局尺寸按新工作区比例恢复后夹取，
  不持久化系统 DPI 或窗口最大化状态；用户缩放只应用一次。
- 核心菜单、面板、关闭提示、组件和属性支持 en-US/zh-CN。稳定 ImGui ID、Reflection
  字段名、实体/资源名和协议字符串不随语言变化。当前 catalog 263 项，包含 39 个核心
  属性标签和 14 个组件标签。底层错误、日志原文和资源文件名保留原文。
- 字体使用系统安装的微软雅黑/宋体，失败回退 Segoe UI/默认字体；无中文字体时禁用
  中文切换并提示。不随仓库分发字体文件。
- Inspector 草稿以项目身份、Scene revision 和 authoring generation 条件提交到既有
  ProjectSession/Command 路径。Enter/有效失焦提交一次，Esc 取消；失败可纠正、重试或
  显式放弃；Agent 改动后旧草稿拒绝覆盖。多行 Text 的 Ctrl/Shift+Enter 保留换行。
  数字输入实时同步 UI 草稿但不实时执行命令，直接 X 关闭也能收集尚未按 Enter 的数字。
  Inspector 在工具栏/层级操作前处理失焦，Play/Save 再检查待提交草稿，失败不使用旧值执行。
  显式放弃使旧输入 ID 失效；Inspector 输入优先于 Scene/全局快捷键和 Game 输入。

## 自动验证

Visual Studio Developer PowerShell，MSVC 14.38，Windows x64：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --parallel 2
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests --parallel 2
out/build/windows-msvc-debug-tests/Tests/JanusTests.exe '[preferences],[locale],[inspector-draft],[workspace]'
ctest --preset windows-msvc-debug-tests
git diff --check
```

两个 preset 配置、构建成功。定向 **25/25，1036 条断言**；最终全量
**482/482，0 failures，38.85 秒**（C 基线 462，本包新增 20 用例）。逐项检查全部
CTest 结果行均为 Passed。追加输入隔离与 locale 修正后再次构建、全量通过。
配置中的可选 PkgConfig/LibUSB 未找到不阻止 SDL 配置；既有
`McpEditorHostTests.cpp:535` 的 C4834 警告仍在，无新增编译失败。

覆盖配置截断/类型/版本/容量/UTF-8/非有限数、原子失败保留字节、LRU、相对项目键、
布局尺寸/模式、Camera 恢复、双语稳定 ID，以及草稿一次 Undo、取消、验证失败保留、
作者态竞争、事务/Runtime 拒绝，以及提交后 Runtime 克隆可见、Stop 后仍能撤销。首轮非法草稿测试曾使用引擎当前允许的 Transform
非有限值，已改为引擎明确拒绝的 `Camera.zoom = 0`；没有改变原有 Transform 校验语义。

生产 Editor 两种协议均通过（每轮 480 个 neutral Step、双方结局、事务/重开/诊断）：

```powershell
$env:JANUS_EDITOR_PREFERENCES_PATH = "$PWD/out/d1-mcp-preferences.json"
python -X utf8 Tests/MCP/integrated_external_e2e.py --host out/build/windows-msvc-debug/Editor/JanusEditor.exe --project Game --era modern --native-editor
python -X utf8 Tests/MCP/integrated_external_e2e.py --host out/build/windows-msvc-debug/Editor/JanusEditor.exe --project Game --era legacy --native-editor
```

原始日志在忽略的 `out/d1-*.log`；生产 E2E 使用其临时项目副本，源码 Game 未修改。

## 本机原生验收

通过 Computer Use 操作真实 SDL/OpenGL Editor，不以源码字符串或无 GPU 模型替代点击。
使用 `out/d1-native-*/SandboxProject` 的 Game 副本；本机系统缩放 150%，窗口截图约
963×631/1707×1019，测试用户缩放 100% 和 125%，并使用系统中文字体。

| 操作 | 观察结果 |
| --- | --- |
| View 切中文，Standard → Focus → Debug | 核心文案可读，Focus 隐藏工具面板，Debug 加大诊断区，窄窗口保留合并页签 |
| 切 125%，关闭重开 | 语言、模式与倍率恢复，没有把 125% 再乘一次 |
| 滚轮改变 Scene 相机，折叠 SpriteRenderer，重开 | JSON 中相机值逐项相等，zoom `0.01963195577263832`；折叠状态恢复，Scene Camera 未变 |
| Hero 位置 -3 输入 -7 后 Esc | 显示恢复 -3，Scene 未移动，无提交 |
| Hero 位置 -3 输入 -6，未 Enter 就点原生 X | 收集为一次编辑并进入中文关闭确认；取消关闭后一次 Undo 恢复 -3 |
| 名称框 Delete、输入中文、Ctrl+Z | 仅影响文本输入；Hero 与 18 个实体保持，未触发场景删除或撤销 |
| 输入“英雄测试”后 Enter、保存、重开 | 层级与 Inspector 名称同步，UTF-8 名称保留 |
| Camera.zoom 输入 0，失焦到组件区 | 显示正数校验错误和重试/放弃入口，旧 Scene 值未改；显式放弃后回到 0.016，无旧值回灌 |
| Camera.zoom 输入 0 直接点 Play | 保留草稿和校验错误，Runtime 保持 Stopped |
| 将本次测试偏好截断为 `{` 后启动 | 英文默认布局启动成功，状态栏/Console 显示回退提示 |
| 将本次测试偏好设为只读，再切网格 | 原子替换失败可见，原文件保留，编辑器仍可操作 |
| 上述写失败状态下创建测试实体、Save、Alt+F4 | 新实体保存成功，Console 出现 Scene saved，窗口正常退出；偏好故障不阻塞退出 |

截图：[中文调试](images/2026-09-08-editor-preferences/debug-zh-inspector.jpg)、
[专注布局](images/2026-09-08-editor-preferences/focus-zh.jpg)、
[数字草稿关闭](images/2026-09-08-editor-preferences/numeric-draft-close.jpg)、
[非法草稿保留](images/2026-09-08-editor-preferences/invalid-draft-retained.jpg)、
[非法草稿阻止 Play](images/2026-09-08-editor-preferences/invalid-draft-blocks-play.jpg)、
[中文/相机/折叠重开](images/2026-09-08-editor-preferences/reopened-chinese-camera-collapse.jpg)、
[损坏回退](images/2026-09-08-editor-preferences/corrupt-preferences-fallback.jpg)、
[偏好故障仍可保存](images/2026-09-08-editor-preferences/scene-save-despite-preference-failure.jpg)。

本次开始时默认偏好文件不存在；测试后清理本轮创建的文件及只读标记，保留 out 下证据副本。
测试使用的名称、实体和错误注入均限于测试副本。

## 后续出口

D1 实现与上述本地验证完成，尚未提交或创建 PR。无新增外部依赖；D2/E/F 待实施。
本机检查不代表全部系统 DPI、跨显示器拖动、长时间 IME 组合输入、缺中文字体设备或首次用户
矩阵通过；Release、目标设备、PM-01 完整关闭矩阵和首次用户验收仍保留发布出口。
不把 D1 或既有 CPU 结果写成 v0.10 已发布。
