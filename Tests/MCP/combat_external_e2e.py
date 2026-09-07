"""Agent regression example: shared Game rules -> neutral runtime.step -> cached snapshot."""
from pathlib import Path
import argparse
import json
import shutil
import tempfile
import sys

sys.dont_write_bytecode = True

from mcp_external_e2e import (
    JanusStdioClient, LEGACY_PROTOCOL, modern_params, require, require_result,
)


def run(host: Path, source: Path, modern: bool) -> None:
    with tempfile.TemporaryDirectory(prefix="janus-combat-") as directory:
        project = Path(directory) / "Game"
        shutil.copytree(source, project)
        manifest = json.loads((project / "project.json").read_text(encoding="utf-8"))
        manifest["defaultScene"] = "Scenes/CombatVerification.scene"
        (project / "project.json").write_text(json.dumps(manifest), encoding="utf-8")
        original = (project / manifest["defaultScene"]).read_bytes()
        client = JanusStdioClient(host, project)
        request_id = 0

        def request(method, params):
            nonlocal request_id
            request_id += 1
            return client.request(request_id, method, modern_params(**params) if modern else params)

        def read(suffix=""):
            result = require_result(request("resources/read", {"uri": "engine://runtime/snapshot" + suffix}), "snapshot")
            return json.loads(result["contents"][0]["text"])

        def call(name, **arguments):
            result = require_result(request("tools/call", {"name": name, "arguments": arguments}), name)
            require(not result.get("isError"), f"Tool failed: {result}")
            return result["structuredContent"]

        def reject(suffix):
            result = request("resources/read", {"uri": "engine://runtime/snapshot" + suffix})
            require("error" in result, f"Invalid query accepted: {suffix}: {result}")

        try:
            if not modern:
                require_result(client.request(0, "initialize", {"protocolVersion": LEGACY_PROTOCOL,
                    "capabilities": {}, "clientInfo": {"name": "CombatVerifier", "version": "1"}}), "initialize")
                client.notify("notifications/initialized")
            require(not read()["available"], "Stopped runtime exposed a snapshot")
            call("runtime.play", startPaused=True)
            initial = read()
            rid = initial["runtimeId"]
            require(initial["fields"]["phase"] == "menu", "Initial phase")
            require(initial["publishedFrameIndex"] == 0, "Initial frame")
            for suffix in ("?unknown=1", "?field=unknown", "?field=phase&field=enemyHp", "?runtimeId=bad"):
                reject(suffix)
            # Reading observes cached data only, and cannot advance the verification script.
            require(read() == initial, "Reading executed gameplay")
            for frame in range(1, 8):
                call("runtime.step")
                snap = read("?runtimeId=" + rid)
                require(snap["runtime"]["state"] == "Paused", "Step changed pause state")
                require(snap["publishedFrameIndex"] == frame, "Publication frame mismatch")
                require(not snap["fields"]["audioAvailable"], "Paused Step opened audio output")
                require(snap["fields"]["musicAudioStatus"] == "Playing", "Loop state unavailable")
                if frame >= 3:
                    require(snap["fields"]["cardAudioStatus"] == "Playing", "Card sound state missing")
                expected_hp = 12 - 4 * max(0, (frame - 1) // 2)
                require(snap["fields"]["enemyHp"] == expected_hp, f"Damage mismatch: {snap}")
            require(snap["fields"]["phase"] == "victory", "Victory missing")
            require(snap["fields"]["lastDamage"] == 4 and snap["fields"]["playerHp"] == 2, "Rule result")
            require(read("?field=enemyHp")["fields"] == {"enemyHp": 0}, "Field filter")
            call("runtime.stop")
            require(not read()["available"], "Stop retained published values")
            reject("?runtimeId=" + rid)
            call("runtime.play", startPaused=True)
            require(read()["runtimeId"] != rid and read()["fields"]["phase"] == "menu", "Restart leaked state")
            reject("?runtimeId=" + rid)
            call("runtime.stop")
            # Failed frames retain the last atomic publication, with exact attempted frame metadata.
            script = project / "Scripts/Combat.lua"
            script.write_text("return { OnUpdate=function(self) Diagnostics.publish_snapshot({phase='partial', hp=8}); error('snapshot fault') end }", encoding="utf-8")
            call("runtime.play", startPaused=True)
            require(not read()["available"], "Unpublished runtime reused old values")
            result = require_result(request("tools/call", {"name": "runtime.step", "arguments": {}}), "fault step")
            require(result["isError"], "Fault fixture did not fail")
            fault = read()
            require(fault["runtime"]["state"] == "Faulted" and fault["runtime"]["partialUpdate"], "Fault metadata")
            require(fault["publishedFrameIndex"] == 1 and fault["fields"]["hp"] == 8, "Fault publication")
            call("runtime.stop")
            require((project / manifest["defaultScene"]).read_bytes() == original, "Runtime changed authoring disk")
        finally:
            client.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--era", choices=("modern", "legacy"), required=True)
    args = parser.parse_args()
    run(args.host.resolve(), args.project.resolve(), args.era == "modern")
    print(f"Combat MCP {args.era}: damage, identity, cache-only reads, fault and cleanup passed.")
