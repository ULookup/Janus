# 编辑器安全关闭：A1/A2 本地实现与验证

日期：2026-09-08。分支 `codex/editor-safe-close`，父基线 main `df00a14`。
记录对象为本轮未提交的工作区改动，不是 main 已集成或发布证据。
方案见[发布准备设计](../superpowers/specs/2026-09-08-v0.10-release-readiness-design.md)。

## 已实现

- ApplicationClient 增加默认 Accept 的 OnCloseRequested，Editor 返回 Defer；SDL 原生关闭
  事件仅发送请求，不提前设置 ShouldClose。确认关闭才退出，普通 RequestExit 语义保持。
- Editor 的 X/Alt+F4 事件与新增 File → Exit 进入同一个关闭控制器，重复请求不重置选择。
  提供 Save Scene and Exit / Discard Scene changes and Exit / Cancel，Esc 取消，默认焦点 Cancel。
- 关闭前处理当前 Inspector 字段；提交失败进入独立草稿决策，允许取消修正或明确放弃字段草稿。
  项目设置按包含名称/路径输入缓冲的完整快照判断未保存，独立选择保存/放弃。
- Runtime 包含 Playing、Paused、Faulted，必须明确同意 Stop；Scene 保存绝不保存 RuntimeScene。
  设置先校验，再 Stop、设置保存、Scene 保存。后续失败保留窗口、错误和未保存数据；已成功的
  Stop/设置保存不回滚，弹窗明确说明。
- session 关闭保护不改变 dirty/history。公共写入、Save/Export、设置保存、运行控制和新事务
  被拒绝；Controller 是唯一可访问私有 Save/Stop 实现的协调器，这些实现仍检查原有 guards。
  控制器析构释放保护且早于所借用 ProjectSession 析构，不引入通用越权 token。
- MCP 分类/授权之后、路由之前拒绝生命周期 busy；不会进入 AbortOwnedRequest。
  读取/诊断继续允许，原事务 owner 仍可 Commit/Rollback，超时和断连清理保持。
- 活动事务退出须先完成或明确回滚；补偿失败禁止保存，只允许取消或明确放弃作者态退出。
- 模态期间屏蔽 Game 输入，发送失焦释放；取消后等待持有按键/鼠标全部释放再恢复游戏输入。
- 最终清理先停止 MCP，再释放关闭控制器、Runtime、面板/项目资源，保持 Renderer 生命周期。
  项目设置面板也在 ProjectSession 之前显式释放。

这是 A 包的关闭实现，没有实现后续 Open/New/SaveAs、Duplicate、Gizmo 或 Release 包。
命名用专注的 EditorCloseController；后续文件生命周期包再按设计扩展离开意图。

## 自动验证

Visual Studio Developer PowerShell，MSVC 14.38，CMake/Ninja，现有依赖版本不变。
先增加失败测试，首次构建在缺少 CloseDecision/EditorCloseController 接口时失败，然后实现。

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --parallel 2
cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests --parallel 2
$env:SDL_AUDIO_DRIVER = 'dummy'
& out/build/windows-msvc-debug-tests/Tests/JanusTests.exe '[close]'
ctest --preset windows-msvc-debug-tests
git diff --check
```

结果：两套 configure/build 成功；新增关闭测试 10 cases / 100 assertions 通过。
最终全量 CTest **427/427，零失败，51.45 秒**。此前一次全量为 51.94 秒；最后增加了
Inspector 只读状态下不重复提交失焦值的保护，两套 Editor 重建后再次运行全量得到上述最终结果。

自动用例覆盖：

| 层 | 已验证行为 |
| --- | --- |
| Application | 默认接受关闭、不再执行额外 client update；重复请求可 Defer 三帧，再显式退出；仅一次 Shutdown |
| Session/Controller | Cancel 保留实体/dirty/history并解除保护；Save 落盘后接受；Discard 不写磁盘 |
| 文件失败 | Scene 目标为目录导致原子替换失败，dirty/history 保留，恢复目标后可重试成功 |
| 设置与 Runtime | 非法设置在 Stop 前拒绝；Paused/Faulted 需确认；设置落盘失败保留 active settings/草稿，已确认 Stop 保持 |
| 事务与恢复 | 未确认回滚不能退出；显式回滚恢复实体/dirty；失败补偿禁止 Save，显式 Discard 退出保留 recovery 直至 teardown |
| MCP | 真正协议 worker 建立 own transaction，关闭期间携 token 的写被拒绝，随后资源仍报告 Active，owner Rollback 仍成功 |

真实生产 Editor 的协议回归也通过（测试自动复制 Game，后台启动窗口）：

```powershell
python Tests/MCP/integrated_external_e2e.py --host out/build/windows-msvc-debug/Editor/JanusEditor.exe --project Game --era modern --native-editor
python Tests/MCP/integrated_external_e2e.py --host out/build/windows-msvc-debug/Editor/JanusEditor.exe --project Game --era legacy --native-editor
```

两种协议均通过 authoring、rollback、reopen、480 Steps、胜负和 diagnostics。
该脚本退出时会对仍存活的 Editor 进程进行 terminate，**不能用它证明关闭弹窗或原生优雅退出**。
本轮没有修改 Game/SandboxProject 内容，没有发布/创建 Tag。

## 仍需原生 PM 验收

本次会话没有可调用的原生桌面交互运行时，未完成下面的真实鼠标/键盘与截图验收。
自动测试证明底层契约和协议回归，不证明 ImGui 控件布局、焦点或 Windows X/Alt+F4 实机路径。
因此 **PM-01 不在本记录中标为正式关闭，A2 的原生验收出口仍未满足**。

1. 项目副本：新增实体 → X → Cancel，实体和 dirty 保留；再次 X → Save，重开保留实体。
2. Alt+F4 和 File → Exit 分别重复 Save/Discard/Cancel；干净场景直接退出。
3. 编辑名称、Text、数值字段时关闭，验证有效值保存、无效值修正/明确放弃及 Esc，不产生重复命令。
4. 设置窗口关闭后仍有草稿，再退出 Editor；覆盖设置保存/放弃，以及 Scene 保存失败后的重试/取消。
5. Playing/Paused/Faulted、活动事务及 recovery 下关闭；确认停止/回滚和错误提示，观察 teardown 日志。
6. 按住游戏键、鼠标捕获、重复 X、取消弹窗，确认输入不穿透/不重放；紧凑窗口与 DPI 下按钮可达。

当前 Inspector 仍沿用原有单活动字段缓冲，未在 A 包扩展为多字段长期草稿或保存点跟踪；
D 包的完整属性编辑和更广泛焦点矩阵继续保留。强制杀进程/断电不在关闭保护保证内。

构建仍有既有 getenv C4996、旧测试返回值 C4834 警告；配置提示可选 PkgConfig/LibUSB 缺失。
没有声称零警告、GPU/音质/Release 或目标设备验收完成。
本地完整日志位于忽略的 `out/close-*.log`，包括两套配置/构建、定向/最终 CTest 和两个 native stdio 结果。
