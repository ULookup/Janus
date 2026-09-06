# Janus Agent Guide

## Mission

Janus is an Agent-native C++20 2D game engine for both human developers and AI agents. The current milestone is **v0.9 Agent Development Loop**. Prefer a complete, testable vertical slice over parallel unfinished subsystems.

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
- Current operation classes are ProjectRead, SceneRead, SceneWrite, and SceneSave. The local Editor stdio policy currently allows the exposed v0.8 operations, but handlers must remain policy-agnostic.
- Entity mutation identity is persistent UUID, never ECS index/generation.
- Scene v1 serialized names remain a compatibility contract.
- MCP Scene writes reuse existing Engine Scene commands. Do not add a parallel direct ECS mutation path.
- During Play, v0.8 write tools remain authoring-read-only while read resources continue to describe EditorScene.
- v0.8 external process tests use the test-only `JanusMcpExternalHost` with `FakeRenderDevice` to remove GPU-driver nondeterminism. Do not turn that fixture into a production MCP executable or duplicate production handlers inside it.
- v0.9 may add runtime control/resources, structured logs, Profiler, Transaction, and Audit/Agent Activity. Extend the existing capability/dispatcher/permission boundaries rather than bypassing them.

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
