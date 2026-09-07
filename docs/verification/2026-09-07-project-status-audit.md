# 项目进度与 PM 文档核对记录

日期：2026-09-07。开始时 main / origin/main 为 `7ecdfb8`，工作区干净；文档更新在
`codex/v0.10-project-status` 分支进行。范围为项目调研、现有能力与产品目标对账、状态及
操作说明更新，没有修改 C++、Lua、场景、依赖、CMake、CI 或版本号。

## 核对范围与结论

- 阅读 PRD、版本路线图、相关技术架构章节、v0.9/v0.10 计划与专项验收。
- 核对 RuntimeExecution / RuntimeSession、ProjectSession / EditorActions、Editor 启动与
  面板、AssetMetadata / ProjectSettings、MCP tools/resources/permission、Game 脚本及测试。
- 核对 CMake 依赖固定版本、构建/测试预设、CI 工作流、Git 历史、远端 main、PR 与发布状态。
- #92 已于北京时间 14:36:08 合并，main 合并后 CI 14:44:11 完成，412/412、47.77 秒。
  查询时无开放 PR、远端 Tag 或 GitHub Release；CMake/MCP 版本仍是 0.9.0。
- v0.10 计划切片和综合示例均已集成；记录 Human Image/Animator 资源赋值、项目/场景/资产
  制作入口、正式测试/Headless/权限等差异，区分待实现和待范围决策，不改变原产品承诺。
- 统一入口见[项目进度与 PRD 对账](../project-status.md)。日期化计划/验收仅添加状态说明，
  保留旧基线、测试数量与原生观察；动画验收额外澄清 Inspector 显示不等于 clip 赋值入口。

远端证据：[PR #92](https://github.com/ULookup/Janus/pull/92)、
[main CI](https://github.com/ULookup/Janus/actions/runs/34091574394)。

## 本轮本地复验

环境：Windows 11 `10.0.26100`，Visual Studio Developer PowerShell，MSVC 14.38.33130 x64。
使用现有 Debug 构建目录进行配置与增量构建，没有清理依赖或重建独立 Release 配置。
两个预设的配置/构建及完整 CTest 输出均已核对，所有命令退出码为 0。

| 命令 | 结果 |
| --- | --- |
| `cmake --preset windows-msvc-debug` | 配置/生成成功 |
| `cmake --build --preset windows-msvc-debug --parallel 2` | 构建成功 |
| `cmake --preset windows-msvc-debug-tests` | 配置/生成成功 |
| `cmake --build --preset windows-msvc-debug-tests --parallel 2` | 构建成功 |
| `ctest --preset windows-msvc-debug-tests` | **412/412，0 failed，32.90 秒** |
| `git diff --check` | 通过 |
| 修改文档的本地链接/代码围栏检查 | 26 个 Markdown 文件、204 个本地链接目标通过；代码围栏闭合 |
| 变更范围与 ignore 检查 | 仅 Markdown；原始日志和核对辅助脚本位于 ignored out/ |

CTest 包含 402 个 C++ 测试及 10 个 MCP 外部进程用例（基础 stdio、Combat、Physics、Prefab、
Integrated 的 modern/legacy），均已逐项核对。文档变更不引入行为，未新增实现测试；本轮
全量结果用于确认当前代码基线，不冒充对全部 PRD 产品入口的覆盖。

仍可见的已有非阻断项：SDL 可选 PkgConfig/LibUSB 未找到；EditorApplication.cpp:114 的
`getenv` C4996，McpEditorHostTests.cpp:451 的 C4834。本次 main CI 还报告
ReflectionRegistry.cpp:230 的 C4458；本地增量构建没有重编该对象，不据此声称该告警消失。

只读远端核对命令包括：

```powershell
git status --short --branch
git log -4 --oneline
git ls-remote origin refs/heads/main 'refs/tags/*'
gh pr list --state open --json number,title,url
gh run view 34091574394 --json status,conclusion,headSha,url,jobs
gh run view 34091574394 --log
gh release list --limit 5
```

本地原始输出位于 `out/pm-audit-2026-09-07/`：`configure-debug.log`、`build-debug.log`、
`configure-tests.log`、`build-tests.log`、`ctest.log`、`main-ci.log`；它们不进入版本控制。

## 验证限制

后续补充：本文记录完成之后，按用户新要求进行了[原生 PM 验收](2026-09-07-pm-editor-acceptance.md)。
下文“本轮”仅指本核对记录；后续 UI 证据和未保存关闭缺陷以新记录为准。

本轮没有重新进行原生窗口操作、真实音频设备/人工听感、GPU/Present 或 Release 性能验证。
这些能力的既有证据及边界仍引用[综合验收记录](2026-09-07-v0.10-integrated-acceptance.md)，
不得把本文 CTest 结果改写为新一轮实机验收。代码检索确认的是当前入口覆盖，不是全仓库
所有错误路径的完整审计。本轮没有发布 Tag/Release，也没有开始 v0.11 实现。
