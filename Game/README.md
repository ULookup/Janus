# Janus Card Combat / 10-05b + 10-06

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
Animation, audio, physics, Prefab and a full Roguelike remain later work packages.
