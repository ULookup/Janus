# Janus Agent Guide

## Mission

Janus is an Agent-native C++20 2D game engine for both human developers and AI agents. The current milestone is **v0.8 MCP Agent Foundation**. Prefer a complete, testable vertical slice over parallel unfinished subsystems.

This file applies to the entire repository. A more deeply nested `AGENTS.md` may add stricter rules for its subtree.

## v0.7 capability baseline

v0.7 Reflection + Command is the completed authoring capability baseline for v0.8.

- `ReflectionRegistry` is explicitly owned by the active host/session; do not introduce a global Reflection singleton.
- Scene persistence and cloning consume the active ReflectionRegistry explicitly.
- `ProjectSession` owns the Human authoring `CommandBus`.
- Editor panels route mutations through `EditorActions`; EditorActions constructs Engine Scene commands instead of mutating reflected ECS state directly.
- MCP must consume Engine `ReflectionRegistry + CommandBus + Scene commands` directly. MCP must not depend on EditorActions or ImGui.
- Entity mutation identity is persistent UUID, never ECS index/generation.
- Scene v1 serialized names are a compatibility contract.
- v0.8 does not introduce Transaction/Audit merely because commands exist; those remain v0.9 scope unless the roadmap is explicitly changed.

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
