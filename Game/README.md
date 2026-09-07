# Janus Card Combat / 10-05b + 10-06 + 10-07

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
Successful card plays now trigger a one-shot green border animation. Invalid plays do not
restart it; Restart restores the base border. The animation does not change combat timing.

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

`Animations/*.clip.json` are version 1 `animation-clip` assets. Animator is editable through
Inspector or the same MCP component commands: add it disabled, assign a clip, then enable.
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
Prefab, integrated v0.10 acceptance and a full Roguelike remain later work packages.


## Physics showcase (10-08)

Open `Scenes/PhysicsShowcase.scene` in the Editor and Play. The left robot falls
through a checkpoint onto the floor; the right robot is removed when it touches
the collector. The left platform moves kinematically. Space applies an upward
impulse, Enter teleports the player back to its initial pose and clears velocity.
The physical boxes match the sprite rectangles, in metres at 64 pixels/metre.

For an independent application, set a copied project's `defaultScene` to this
scene or supply `ProjectRuntimeConfig.startupScenePath`; the default combat scene
is preserved. Paused Step runs one neutral 1/60-second physics tick.

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
