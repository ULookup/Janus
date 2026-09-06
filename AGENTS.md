# Janus Agent Guide

## Mission

Janus is an Agent-native C++20 2D game engine for both human developers and AI agents. The current milestone is **v0.10 Game Systems**. Stage A: Project Settings + Action Input is integrated; work package 10-03: Shared Runtime Execution is implemented and locally verified. The next work package is 10-04a: UI layout and images. Prefer a complete, testable vertical slice over parallel unfinished subsystems.

This file applies to the entire repository. A more deeply nested `AGENTS.md` may add stricter rules for its subtree.

## v0.8 capability baseline

v0.8 MCP Agent Foundation is complete and is the capability baseline for v0.9.

- `ReflectionRegistry` remains explicitly owned by the active host/session; do not introduce a global Reflection singleton.
- Scene persistence and cloning consume the active ReflectionRegistry explicitly.
- `ProjectSession` owns the shared Human/Agent authoring `CommandBus`.
- Human Editor panels route through `EditorActions`; MCP never depends on EditorActions or ImGui.
- Native `JanusMCP` owns JSON-RPC/stdio protocol, ToolRegistry, ResourceRegistry, Reflection schema adaptation, Scene Resources/Tools, dispatcher primitives, and permission abstractions.
- Production Agent entry is `JanusEditor --project <path> --mcp-stdio`.
- The primary MCP protocol era is `2026-07-28`; `2025-11-25` stdio initialize compatibility remains supported.
- stdout is protocol-only in MCP stdio mode. Diagnostics/logging must use stderr or another non-protocol sink.
- MCP I/O workers may parse/serialize protocol data but must not mutate EditorScene, CommandBus, Reflection-backed authoring state, dirty state, or save state directly.
- Live Editor resource/tool handling crosses `McpMainThreadDispatcher` before permission enforcement and capability routing.
- Operation classification is an explicit whitelist: ProjectRead, SceneRead, SceneWrite, SceneSave, RuntimeRead, RuntimeControl, DiagnosticsRead, TransactionControl, and ActivityRead. Unclassified operations are denied. Handlers remain policy-agnostic.
- Entity mutation identity is persistent UUID, never ECS index/generation.
- Scene v1 serialized names remain a compatibility contract.
- MCP Scene writes reuse existing Engine Scene commands. Do not add a parallel direct ECS mutation path.
- Playing, Paused, and Faulted retain a Runtime and block authoring writes. Existing Scene resources describe EditorScene; runtime/entity reads RuntimeScene. Step is fixed 1/60 second with neutral input and no reload.
- v0.8 external process tests use the test-only `JanusMcpExternalHost` with `FakeRenderDevice` to remove GPU-driver nondeterminism. Do not turn that fixture into a production MCP executable or duplicate production handlers inside it.
- v0.9 implements Runtime control/resources, shared logs, CpuProfiler, transactions and Agent Activity. See docs/verification/2026-09-06-v0.9-agent-development-loop.md for local verification and integration status.
- EditorActions and MCP SceneTools use ProjectSession authoring entry points. Transaction owner/token guards, Runtime guards, dirty restoration and recovery cannot be replaced by UI disabling.
- CommandBus groups already-executed pending commands at Commit; rollback preserves the redo tail. Limits are 64 commands / 60 seconds / 8 MiB conservative undo reservation. Save and Runtime are outside transactions.
- Compensation failure freezes authoring until explicit Human discard/reload. MCP must refresh Scene bindings after a scene revision changes. Cleanup occurs on the host owner thread.
- Diagnostic stores are bounded and session-owned; Core profiling/commands must not depend on Scene, Renderer, Editor or JSON. Never treat CPU timing as GPU timing.

## v0.10 Stage A constraints

- Engine/Project owns versioned project settings loading, validation and atomic saving. Missing project.json preserves legacy defaults; malformed configuration must fail explicitly. ProjectRuntimeConfig path overrides are optional and take precedence over the manifest.
- ProjectSession guards settings saves during Runtime, transactions and recovery. Project settings are outside Scene Undo/transactions and must not change Scene dirty/history; file-write failure preserves the active settings.
- ScriptEngine owns a snapshot of input bindings supplied by the host. Core input defines no game-specific action names and does not depend on JSON, Scene or Editor.
- Action state aggregates bound keys across frame boundaries. Paused Step remains neutral input at 1/60 second. Native keyboard APIs remain compatible.
- Editor gameplay input is limited to the displayed Game View; pointer coordinates use the project's logical resolution. Tool panels and letterboxing must not send gameplay input.
- See docs/superpowers/specs/2026-09-06-v0.10-project-input-design.md for settings apply timing and scope. Stage A does not complete v0.10; shared execution is covered below, while fixed-tick scheduling and UI/game systems remain subsequent work.

## v0.10 shared Runtime constraints

- Managed Application and RuntimeSession use Engine/Runtime/RuntimeExecution for script startup, input snapshots, optional reload, update and shutdown. Do not add a second host-specific simulation path for new systems.
- RuntimeExecution owns its stable InputState and ScriptEngine; the host owns Scene and AssetService and must keep them alive until execution teardown. It does not clone Scene or implement a host state machine.
- Application preserves client.OnUpdate before simulation and client.OnShutdown before script shutdown. RuntimeSession preserves Clone, Faulted retention and RuntimeStatus; do not force the independent application to retain an Editor-style faulted world.
- Paused Step passes neutral input, fixed 1/60 second and ScriptReloadPolicy::Skip. Invalid/stopped Advance must not overwrite the initial input snapshot or run scripts. Future fixed-tick scheduling remains a separate Physics design task.
- See docs/superpowers/specs/2026-09-07-v0.10-shared-runtime-design.md and docs/verification/2026-09-07-v0.10-shared-runtime.md for the 10-03 boundary and validation. UI and the remaining v0.10 game systems are not implemented by this refactor.

## Source of truth

Before changing architecture or scope, read the relevant files under `docs/`:

- `docs/Janus Engine 产品需求文档（PRD）.md`
- `docs/Janus Engine 技术架构设计.md`
- `docs/Janus Engine 版本路线图.md`

When documents disagree, do not silently choose one. Report the conflict and keep the change within the currently requested milestone.

## Required workflow

1. Inspect the current Git status and preserve unrelated user changes.
2. Search with `rg` and `rg --files`; exclude generated directories such as `out/`.
3. State the intended scope before changing public APIs or architecture.
4. Add or update automated tests before implementing behavior changes.
5. Configure, build, and test with the repository presets.
6. Read the full verification output before claiming completion.
7. Report commands run, results, and any remaining failures.

## Architecture boundaries

- Game, Editor, CLI, and MCP are clients of the Engine public API.
- Core must not depend on Renderer, Scene, Asset, Editor, MCP, or Game.
- Platform-specific behavior remains behind platform interfaces.
- Renderer and gameplay code must not expose SDL or graphics-backend types through public APIs.
- CMake implementation sources (`.cpp`) are `PRIVATE`; only intentional API files are `PUBLIC`.
- Use explicit `Result`/`Error` handling for recoverable failures. Do not use exceptions as routine control flow.
- Keep ownership explicit and prefer RAII for native resources.
- Do not introduce mutually dependent global `*Manager` objects.
- Keep Engine free of game-specific behavior.

## Build and test

Run commands from Visual Studio Developer PowerShell or Developer Command Prompt.

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug

cmake --preset windows-msvc-debug-tests
cmake --build --preset windows-msvc-debug-tests
ctest --preset windows-msvc-debug-tests
```

For a behavior change, run the narrowest relevant test first and then the full `windows-msvc-debug-tests` build and CTest preset.

## Coding conventions

- Language level: C++20.
- Namespace: `Janus` and focused nested namespaces.
- Types and functions: PascalCase, matching the existing code.
- Member fields: `m_` prefix; static fields: `s_` prefix.
- Use fixed-width aliases from `Core/Types.h` where engine-facing width matters.
- Include headers by their path relative to `Engine/`.
- Apply `.clang-format` to touched C++ regions only; do not reformat unrelated files.
- Write concise comments for critical intent and invariants—especially ownership/lifetime, cleanup order, backend translation, and non-obvious algorithms. Explain why the constraint exists; do not restate what the code already says.
- Keep warnings enabled through `janus_set_warnings` for every Janus target.

## Scope control

- Do not add Renderer, ECS, Scene, Asset, Lua, Editor, Reflection, Command, or MCP work unless the task explicitly requests it.
- Do not add a dependency without explaining its role, boundary, version pin, and test impact.
- Do not add speculative abstractions for Vulkan, 3D, jobs, networking, or plugins.
- Do not edit generated dependency sources under `out/`.

## Repository hygiene

- Never commit `.vs/`, `out/`, generated projects, binaries, logs, or local settings.
- Do not overwrite or delete unrelated uncommitted changes.
- Keep commits focused and use imperative Conventional Commit-style subjects where practical.
- Do not select or add an open-source License without owner approval.

## Definition of done

A change is complete only when:

- requested behavior and documentation agree;
- relevant automated tests pass;
- both configure and build succeed for the affected preset;
- `ctest --preset windows-msvc-debug-tests` reports zero failures for behavior changes;
- `git diff --check` reports no whitespace errors;
- generated files remain ignored;
- the final report names verification commands and any known limitations.
