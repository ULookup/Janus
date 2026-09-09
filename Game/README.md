# Janus Robot Card Arena / v0.10 integrated sample

Status (2026-09-07): integrated into main `7ecdfb8` through PR #92; post-merge Windows CI passed 412/412. v0.10 is not released. Current implementation gaps and release gates are tracked in the [project status](../docs/project-status.md). This is a fixed-battle systems sample, not the completed v0.11 Roguelike.

This disk-backed game uses the existing shared Runtime, Canvas, Text and Button APIs.
All combat rules are in [Combat.lua](Scripts/Combat.lua); Engine contains no card or HP rules.

From the repository root after building the Debug preset:

```powershell
./out/build/windows-msvc-debug/Sandbox/JanusSandbox.exe ./Game
./out/build/windows-msvc-debug/Editor/JanusEditor.exe --project ./Game
```

In Editor, press Play in the toolbar to interact with Game View. Click **Start Battle**,
select **Strike**, then **Play Selected Card**. Three strikes win: enemy HP 12 → 8 → 4 → 0.
The enemy returns 2 damage after each nonlethal turn. Three **Wait** cards lose.
**Restart to Menu** restores HP, turn, selection and damage. Up/Down select a button;
Enter/Space confirm. Controls outside their valid phase do nothing; select a card each turn.
Pause/Step does not click buttons. Stop restores the EditorScene's original text.
Buttons display their disabled color outside valid phases: only Start works in the menu;
Strike/Wait and Restart work in battle, Play requires a selected card, and only Restart
works after victory or defeat. Combat.lua also rejects invalid keyboard/direct actions before
audio, animation or arena feedback. Hot reload resets both phase and button availability.
The generic Lua method `Entity:set_button_interactable(bool)` changes the bound runtime
Button only; it rejects non-boolean arguments and entities without a Button. It does not
change the saved authoring scene or introduce game rules into Engine.
Successful card plays now trigger a one-shot green border animation. Invalid plays do not
restart it; Restart restores the base border. The animation does not change combat timing.

## Integrated arena

The default `Scenes/Integrated.scene` adds two expanded `Prefabs/Fighter.prefab` subtrees,
independent Animator playback, native rigid bodies, a floor and landing/impact feedback.
`Combat.lua` still owns every damage/turn rule. `Arena.lua` composes systems; a strike
kicks the enemy upward, and retaliation kicks the hero. Physics does not change damage timing.
Reloading `Combat.lua` resets the battle while preserving Arena bindings and actor references.
`0` resets both bodies and animations as well as combat. `1` starts, `2` selects Strike,
`3` selects Wait, `4` plays. In Editor, move the pointer into the displayed Game View
and focus it; tool panels and letterboxing do not supply gameplay input.

World and UI use the project's 1280×720 logical resolution even when Game View is smaller.
`Scenes/Combat.scene` retains the original UI-only layout. Other showcases remain separate.
Prefab instances are expanded authoring entities; editing the template does not update old instances.
The two named actors are already configured copies; newly instantiated Fighters are independent props.

The integrated snapshot adds `physicsTick`, `droppedSeconds`, `bodies`, `heroY`, `enemyY`,
`hits`, `landings`, `enemyAnimating`, `enemyFrame`, `verifiedVictory`, `verifiedDefeat`.
Physics/animation observation is from the preceding completed tick; action feedback counts are current.
`audioAvailable` means the output device exists, including while paused; it does not mean sound is queued.

For Agent acceptance, copy Game and set `defaultScene` to `Scenes/IntegratedVerification.scene`.
Start paused and Step 480 times: frame 181 is victory, frame 421 is defeat; the last snapshot
records both outcomes. This calls the same rule function as Human buttons and never injects input.
Run the repeatable authoring/rollback/reopen/physics/diagnostics task:

```powershell
python Tests/MCP/integrated_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era modern
python Tests/MCP/integrated_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era legacy
$env:SDL_AUDIO_DRIVER = 'dummy'
./out/build/windows-msvc-debug-tests/Tests/JanusGameSystemsBenchmark.exe ./Game
```

The benchmark warms 180 frames and measures 720 frames at 1/60 s. Its fake backend measures
CPU simulation/render submission, not GPU/present latency. See the [integrated verification](../docs/verification/2026-09-07-v0.10-integrated-acceptance.md)
for the original measurements; use project status for current merge/release information. Editor's Profiler and `engine://profiler/latest-frame`
expose shared Runtime stages. Application's `GetProfiler()` returns completed CPU frame copies via `Latest()`.

## Audio

The menu and battle share a quiet background loop; a valid card play triggers one short
tone. Restart stops the card tone and restarts the music. Runtime Pause/Faulted clears
pending output; Step advances playback state silently; Resume continues from that cursor.
Stop releases all voices and the device. An unavailable device produces a diagnostic and
silent playback without preventing the project from opening or running.

AudioSource stores `clip`, `enabled`, `playOnStart`, `volume` (0–1), and `loop`. Add it
disabled, assign an `audio-clip` via Inspector/Asset Browser or shared MCP commands, then
enable it. `assets.search` supports `type="audio-clip"`. Clips are PCM16 WAV, mono/stereo,
8–96 kHz, up to 120 seconds and 32 MiB. They are decoded into shared immutable CPU data.

Lua entity methods: `play_audio([clip UUID])`, `pause_audio()`, `resume_audio()`,
`stop_audio()`, `set_audio_volume(value)`, `set_audio_loop(bool)`. `audio_state()` returns
status (`Playing`/`Paused`/`Stopped`), cursor seconds, volume and loop. `audio_output()`
returns device availability and an error string (empty if none); availability is false
before the first audible Advance because opening the device is lazy.

Game snapshots include `cardAudioStatus`, `cardAudioCursor`, `musicAudioStatus`,
`musicAudioCursor`, `audioAvailable`, and `audioError`. Lua publishes before this frame's
Audio Advance; cursors describe logical simulation playback, not measured speaker output.

Run `python tools/generate_demo_audio.py` to recreate the original [tones](Audio/README.md).
The device-independent tests use fake sinks and SDL's dummy driver. To check the local
default device explicitly after building the tests preset, run:

```powershell
./out/build/windows-msvc-debug-tests/Tests/JanusAudioDeviceSmoke.exe
```

This plays three quiet one-second tones and checks open/submit/clear/close. See the
[audio design](../docs/superpowers/specs/2026-09-07-v0.10-audio-design.md) for the 64-source,
100 ms output-block and 200 ms queue bounds. Streaming codecs, spatial audio, DSP, and
music-grade clock synchronization are outside this first slice.

## Animation showcase

To inspect the world Sprite separately, copy Game and set the copy's `project.json`
`defaultScene` to `Scenes/AnimationShowcase.scene`. Open the copy with either executable.
The robot loops automatically; **Space** stops and restores its base frame, **Enter** replays
the configured loop, and **D** switches to a short one-shot that holds its final frame.
Editor keyboard input requires Game View focus while playing.
Pause freezes playback; Step advances 1/60 second; Stop restores authoring state.
The sample publishes `playing`, `animationFrame` and `elapsed` in the existing snapshot
resource during Lua Update, before that frame's animation advance.

`Animations/*.clip.json` are version 1 `animation-clip` assets. Animator enabled/playOnStart/speed
are editable in Inspector. This branch adds a typed clip picker and an AnimationClip filter/assignment
action in Asset Browser. To configure a new clip through the shared MCP commands, add Animator disabled, find a clip
with `assets.search`, set component `Animator`, property `clip` to that asset UUID through
`scene.set_component_property`, then set `enabled` to true. These commands remain Human-undoable.
Inspector also provides a typed Image.texture picker. See the editor UI verification record for branch validation and remaining release gaps.
`assets.search` accepts `type="animation-clip"`. Lua provides `play_animation([clip UUID])`,
`stop_animation()` and `animation_state()` returning playing, zero-based frame and elapsed seconds.
See the [animation design](../docs/superpowers/specs/2026-09-07-v0.10-animation-design.md).
Run `python tools/generate_demo_animation.py` from the repository root to recreate the
original geometric atlas and clips with the Python standard library. No external art is used.

## Agent observation

The production MCP entry remains `JanusEditor --project ./Game --mcp-stdio`.
Read `engine://runtime/snapshot` for the last explicitly published snapshot, including
`runtimeId`, `publishedFrameIndex`, Runtime status and these Game-owned fields:

| Field | Meaning |
| --- | --- |
| phase | menu / battle / victory / defeat |
| enemyHp, playerHp | Current HP |
| turn | Completed turns, starting at 0 |
| lastDamage, lastRetaliation | Last resolved damage to enemy/player |
| selectedCard | none / strike / wait |
| canPlay | Whether a selected card can be played now |

Optional `?runtimeId=<uuid>&field=enemyHp` restricts the read. Old runtime IDs and unknown
fields fail explicitly. Stopped or unpublished sessions return `available=false`.
Reads inspect cached scalar data and never execute Lua. A fault can retain a publication
from the failed frame; inspect `runtime.partialUpdate` and `runtime.failedFrameIndex`.
Reload leaves the last publication until a script explicitly replaces it; Stop clears it.
The diagnostic map is a single last-publication slot per Runtime, not a history or save game.
Strings use the same valid UTF-8/control-character rules as Text; only CR/LF controls are accepted.

For a deterministic Agent regression, copy this project and set `defaultScene` in the copy's
project.json to `Scenes/CombatVerification.scene`. Start paused and run seven neutral Steps:

| Step | Action | Enemy HP | Phase |
| --- | --- | --- | --- |
| 0 | Startup | 12 | menu |
| 1 | Start | 12 | battle |
| 2 | Select Strike | 12 | battle |
| 3 | Play | 8 | battle |
| 4–5 | Select / Play | 4 | battle |
| 6–7 | Select / Play | 0 | victory |

This scenario calls the same rule function as OnClick. It tests rules and observation;
the pointer/keyboard tests and native window checks cover UI interaction separately.

The executable regression example copies the project automatically and also checks identity,
unknown fields, fault retention, Stop cleanup and unchanged authoring files:

```powershell
python Tests/MCP/combat_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era modern
python Tests/MCP/combat_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era legacy
```

The FakeRenderDevice host is test-only. No production headless or MCP input injection was added.
Font files are copies of the existing project-authored [demo font](Fonts/README.md).
After regenerating the Sandbox font, copy its JSON and PNG into Game/Fonts as well.
No new dependencies, downloaded artwork, or licensing changes are involved.
Integrated v0.10 acceptance is merged. A full Roguelike, save data, multiple-scene gameplay
and networking remain future product work.


## Physics showcase (10-08)

To use `Scenes/PhysicsShowcase.scene`, copy Game, set the copy's `project.json.defaultScene`
to that path, reopen the copied project in Editor and Play. The current Editor has no general
scene-file switching UI. The left robot falls
through a checkpoint onto the floor; the right robot is removed when it touches
the collector. The left platform moves kinematically. Space applies an upward
impulse, Enter teleports the player back to its initial pose and clears velocity.
The physical boxes match the sprite rectangles, in metres at 64 pixels/metre.

For an independent application, set a copied project's `defaultScene` to this
scene or supply `ProjectRuntimeConfig.startupScenePath`; the original project's default
Integrated scene is preserved. Paused Step runs one neutral 1/60-second physics tick.

The existing `engine://runtime/snapshot` reports tick, droppedSeconds, bodies,
x/y, vx/vy, collisions, triggerEnters/Exits and rayFloor. Update publishes the prior
completed tick, so 180 Steps report tick=179, one checkpoint entry/exit, floor
contact, five remaining bodies, and player y approximately zero. Stop/Play restores
all six authored bodies and resets counters.

```powershell
python Tests/MCP/physics_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era modern
python Tests/MCP/physics_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era legacy
```

Only root unit-scale rectangular bodies are supported in this slice. Physics
configuration is authored before Play; changes to active body/shape settings fail
explicitly. Lua runtime methods and full limits are specified in the physics design.


## Prefab showcase (10-09)

In a Game copy, set `project.json.defaultScene` to `Scenes/PrefabShowcase.scene`, reopen
that project and Play to see two independent robot subtrees.
Each root owns SpriteRenderer, LuaScript and Animator; its Badge child retains local placement.
Select `Prefabs/Robot.prefab` in Assets and click **Instantiate Prefab** to add another robot.
Move the new root with Inspector to separate overlapping instances. **Undo** removes the whole
instance and **Redo** restores the same IDs/content. Select a robot root in Hierarchy and click
**Export Prefab** to save a new `Prefabs/<asset UUID>.prefab` registered in the project.
Export preserves scene dirty/history and is unavailable during Runtime or a transaction.

Prefab v1 expands into ordinary Scene entities; template edits do not propagate to instances.
There are no nested Prefabs, overrides, apply/revert or runtime spawn APIs in this stage.
Asset UUIDs refer to this project's registry; copying a template alone into another project
requires registering its referenced assets with matching identities and types.
A template containing a Canvas or primary Camera must not conflict with the destination scene.

The script moves each instance relative to its own starting position. The existing runtime
snapshot publishes the last updating robot's UUID, elapsed time and animation status; it is
not an aggregate Prefab state. Save/reopen retains expanded instances, including child order.

```powershell
python Tests/MCP/prefab_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era modern
python Tests/MCP/prefab_external_e2e.py --host out/build/windows-msvc-debug-tests/Tests/JanusMcpExternalHost.exe --project Game --era legacy
```

Agent authoring uses `scene.export_prefab {entity}` and `scene.instantiate_prefab {asset}`;
instantiation accepts the existing optional `transaction` UUID. Export returns a new asset
UUID; instantiate returns the new root entity UUID. Use `assets.search` with type `prefab`
and the existing scene/entity resources to inspect results. Runtime still uses shared
RuntimeExecution, with neutral paused Step and isolated authoring state.
