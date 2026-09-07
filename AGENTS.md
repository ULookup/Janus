# Janus Agent Guide

## Mission

Janus is an Agent-native C++20 2D game engine for both human developers and AI agents. The current milestone is **v0.10 Game Systems**. Local main is synchronized to origin/main `b9b990b` (fetched 2026-09-07), including UI/Button, Combat/Animation (#91 dependency chain), Audio (#89), and Physics (#90). **10-09 Prefab Foundation** and **10-10 integrated acceptance** are implemented locally on `codex/v0.10-acceptance`, based on that main. See docs/superpowers/plans/2026-09-07-v0.10-integrated-acceptance.md and docs/verification/2026-09-07-v0.10-integrated-acceptance.md for current evidence. Earlier main/dependency-branch and next-stage descriptions are historical. Next is focused review/integration and release preparation, not a new unfinished subsystem. The integration review is recorded in docs/verification/2026-09-07-v0.10-integration-review.md; use Git/PR state for merge status. v0.10 is not released. Prefer a complete, testable vertical slice over parallel unfinished subsystems.

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

## v0.10 UI layout constraints

- Engine/UI uses one root screen-space Canvas and the host project logical resolution. UIRect ignores world Transform; unsupported nested/multiple canvases fail explicitly.
- Canvas/UIRect/Panel/Image authoring uses the active ReflectionRegistry, Scene v1 persistence/cloning, Inspector and shared commands. No parallel UI serialization or direct MCP ECS writes.
- UILayoutResult defines paint and reverse geometric hit order. UI batching combines only adjacent compatible sprites; world texture sorting must never reorder UI. CPU clipping adjusts both rectangles and UVs.
- Renderer2D owns its lazy solid-color texture; overlay projection must be committed through UseShader before drawing. World and UI share one clear, target lifetime and cumulative statistics.
- Reparent preserves local fields and restores parent UUID/sibling order on Undo. Human EditorActions and scene.reparent_entity use ReparentEntityCommand and ProjectSession guards. Root order remains UUID order.
- UI preview is in Game View. Text and offline font assets are implemented by 10-04b; Button/events are implemented by 10-05a; the playable combat sample remains 10-05b. See docs/superpowers/specs/2026-09-07-v0.10-ui-layout-design.md and docs/superpowers/specs/2026-09-07-v0.10-ui-text-design.md.

## v0.10 Text constraints

- Font v1 uses a registered Texture atlas and bounded, validated Unicode glyph metrics. AssetCache owns CPU Font data; AssetService owns lifetime and atlas invalidation. Font never owns a second GPU texture.
- Text content is valid UTF-8, at most 4096 bytes; missing glyphs use the declared fallback. Explicit newlines, per-line left/center/right alignment, own-rect and parent clipping are supported; shaping, auto wrapping and full Unicode font coverage are not.
- Text authoring follows active ReflectionRegistry, Scene v1, shared commands and guards. Lua get_text/set_text operate on the bound Runtime Scene through existing shared execution.
- assets.search is a bounded read-only AssetRegistry query classified as ProjectRead. It must use the existing main-thread dispatch/permission path and never dirty or load the project.

## v0.10 Animation constraints

- AnimationClip v1 is a bounded CPU asset referencing one registered Texture atlas; playback owns a value copy and no second GPU texture.
- Animator stores only clip/enabled/playOnStart/speed through active Reflection, Scene v1 and shared commands. RuntimeExecution owns UUID-keyed AnimationSystem cursors.
- Advance runs Lua before Animation. Renderer and UI layout read transient poses without changing SpriteRenderer/Image authoring fields.
- Play/Switch validates before replacing playback and holds frame 0 for its issuing Advance. Stop restores the base pose; natural one-shot completion holds the last frame. Pause freezes; neutral Step advances 1/60 without automatic asset reload; a new Start refreshes configured clips.
- See docs/superpowers/specs/2026-09-07-v0.10-animation-design.md and docs/verification/2026-09-07-v0.10-animation.md. State machines and animation editors remain out of scope.

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

## v0.10 Button constraints

- UIInteraction is owned by RuntimeExecution; hover/focus/capture are transient and never serialized. Button uses shared Reflection/Command authoring and fixed same-entity Lua OnClick.
- Consume UI input before Lua Update, dispatch UUID events on the owner thread, and preserve game release continuity. Do not replace ordered input with final-frame pointer hit testing.
- Captures cancel on drag-out, invalid targets, focus loss and Pause/Stop; consumed gestures remain quarantined until release. Prime Start/Resume so paused input is not replayed. Neutral Step never dispatches UI.
- See docs/superpowers/specs/2026-09-07-v0.10-ui-button-design.md. ButtonShowcase is a click-counter demonstration; Game/ contains the separate 10-05b combat slice.

## v0.10 Combat and snapshot constraints

- Game/Scripts/Combat.lua owns fixed battle rules; Engine must not define Card/Health or game-specific state.
- Diagnostics.publish_snapshot atomically replaces one bounded scalar map per ScriptEngine. Reads never execute Lua or traverse arbitrary script tables. RuntimeExecution supplies the run UUID and publication frame; RuntimeSession reuses that identity.
- Invalid publication preserves the previous value. Stop/startup failure clears values; Faulted retains the last publication with attempted-frame metadata. Snapshots are transient and never enter Scene serialization, commands or dirty/history.
- engine://runtime/snapshot is RuntimeRead through the existing owner-thread dispatcher and permission path. Unknown fields/old runtime IDs fail; no MCP input injection.
- CombatVerification.scene calls the same game rule function during neutral Update/Step; it validates rules and observation, separately from pointer/keyboard UI tests.
- See docs/superpowers/specs/2026-09-07-v0.10-playable-combat-design.md and docs/verification/2026-09-07-v0.10-playable-combat.md.


## v0.10 Audio constraints

- AudioClip is bounded PCM16 WAV CPU data (mono/stereo, 8–96 kHz, <=120 seconds / 32 MiB), shared immutably by AssetCache and playback. Unload cannot invalidate active voices; new Runtime Start refreshes configured clips.
- AudioSource persists only clip/enabled/playOnStart/volume/loop through active Reflection, Scene v1 and shared commands. Runtime UUID-keyed voices and device state are transient.
- RuntimeExecution owns AudioSystem and its lazy device, runs Lua then Physics then Animation then Audio, and destroys Lua before audio resources. SDL 3.4.14 stays private behind AudioDevice; device threads never read Scene or Lua.
- At most 64 enabled sources; Play validates before replacement, Pause retains cursor, Resume respects per-source pause, Stop resets. Volume/loop Lua controls affect runtime state only.
- Runtime Pause/Faulted clears queued output. Neutral Step advances logical cursors silently; startPaused never opens output. Stop/startup failure releases voices and device. Device failure is a bounded observable silent fallback; content errors retain normal Runtime failure semantics.
- Mixer output is 48 kHz stereo, at most 100 ms per Advance, queued at most 200 ms. Cursors follow simulation time, not a measured hardware clock. No streaming/spatial/DSP or music synchronization guarantee.
- See docs/superpowers/specs/2026-09-07-v0.10-audio-design.md and docs/verification/2026-09-07-v0.10-audio.md. Game owns sample sounds and publishes playback fields through the existing snapshot path.


## v0.10 Physics constraints

- Box2D 3.1.1 is pinned by commit/hash and PRIVATE to Engine. PhysicsSystem owns one single-threaded world; public APIs expose UUIDs and Janus values only.
- RigidBody2D/Collider2D use active Reflection, Scene v1 and shared authoring commands. Enabled bodies require a box collider and root unit-scale Transform. Units are metres, +Y up, radians; gravity (0,-9.81). Body/shape configuration is immutable while active; restart to apply edits.
- RuntimeExecution starts physics before Lua OnCreate, then runs UI/Lua Update once per Advance, Physics fixed ticks, Animation and Audio. Tick = 1/60, four solver substeps, maximum eight catch-up ticks; discard excess whole time, preserve fractional time. Neutral paused Step still advances one tick, with no input/reload.
- Native poses update RuntimeScene Transform only. Explicit Lua set_position teleports physical entities; velocities/impulses, accumulator, contacts and world ids never serialize or dirty authoring state.
- Copy Box2D events after each solver step into sorted UUID pairs. Lua OnCollisionEnter/Exit and OnTriggerEnter/Exit run on the owner thread. Entity:destroy queues deletion; skip pending Update/physics callbacks, invoke OnDestroy while entities still exist, then remove Scene subtrees and native bodies even if a callback fails. OnDestroy may queue another batch for the next safe boundary.
- Limits: 1024 bodies/pending requests, 4096 raw events per tick, finite bounded geometry/controls. Destroyed contacts cancel without synthetic exits. Raycasts use start/end, exclude triggers by default, and ignore initial overlaps per Box2D.
- See docs/superpowers/specs/2026-09-07-v0.10-physics-design.md and docs/verification/2026-09-07-v0.10-physics.md. PhysicsShowcase is independent of combat. Prefab and integrated v0.10 acceptance remain outstanding.


## v0.10 Prefab constraints

- Prefab v1 is a bounded, closed single-root subtree containing Scene v1 data (1 MiB / 1024 entities / 64 hierarchy levels). Use active ReflectionRegistry and shared subtree snapshot capture/restore, never a second component codec.
- Every instance remaps entity and parent UUIDs, preserves sibling order/local fields, AssetReference UUIDs and strings. There is currently no reflected EntityReference type; adding one requires explicit remapping and external-reference policy.
- Instances expand into ordinary Scene entities. No source link, nested Prefab, override/apply/revert, propagation or runtime spawning in this slice. New registered instances validate referenced asset identities/types; nil references retain Scene semantics.
- InstantiatePrefabCommand is one shared authoring command with bounded undo reservation. Redo restores the original immutable snapshot and UUIDs without disk reload. Reject multiple primary cameras and unsupported Canvas structures without changing existing entities.
- ProjectSession exports a fresh UUID file then atomically saves registry metadata and publishes the in-memory registry. Failed registry save removes the fresh file; a process crash between writes may leave an unregistered orphan. Export is outside scene dirty/history and forbidden during Runtime/transactions/recovery.
- Human EditorActions and MCP share ProjectSession guards. scene.export_prefab is SceneSave; scene.instantiate_prefab is SceneWrite through existing owner-thread dispatch. Game/Prefabs/Robot.prefab and PrefabShowcase demonstrate Sprite, Lua, Animator and child hierarchy.
- See docs/superpowers/specs/2026-09-07-v0.10-prefab-design.md and docs/verification/2026-09-07-v0.10-prefab.md. Integrated v0.10 gameplay/device/performance acceptance remains 10-10.


## v0.10 integrated acceptance constraints

- Game/Scenes/Integrated.scene composes expanded Fighter prefab instances, UI/card rules, animation/audio and physics feedback. Combat.lua owns rule resolution; Arena.lua owns composition and observation. Physics feedback does not decide damage.
- IntegratedVerification.scene calls the same Game action entry on a deterministic schedule. Production scenes never synthesize gameplay input; MCP Step remains neutral.
- RuntimeExecution borrows an optional host-owned CpuProfiler. UI, Reload, Lua, Physics, Animation and Audio scopes are CPU timings, including callback work within its owning stage. The recorder must outlive execution. Application profiling excludes event polling, client update, Present and pacing.
- Game rendering uses logical project resolution for both world projection and UI, independently of render-target pixel dimensions. Scene View keeps its target-sized editor camera. Renderer2D accepts optional projectionViewport; zero dimensions fall back to target size.
- 10-10 evidence is local acceptance, not a merge/release declaration. See the integrated verification record for CPU baseline limits and actual native observations.
