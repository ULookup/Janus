"""Real Box2D simulation observed through the existing owner-thread MCP path."""
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
    with tempfile.TemporaryDirectory(prefix="janus-physics-") as directory:
        project = Path(directory) / "Game"
        shutil.copytree(source, project)
        manifest_path = project / "project.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["defaultScene"] = "Scenes/PhysicsShowcase.scene"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        scene_path = project / manifest["defaultScene"]
        original = scene_path.read_bytes()
        client = JanusStdioClient(host, project)
        request_id = 0

        def request(method, params):
            nonlocal request_id
            request_id += 1
            return client.request(request_id, method, modern_params(**params) if modern else params)

        def read():
            result = require_result(request("resources/read", {"uri": "engine://runtime/snapshot"}), "snapshot")
            return json.loads(result["contents"][0]["text"])

        def call(name, **arguments):
            result = require_result(request("tools/call", {"name": name, "arguments": arguments}), name)
            require(not result.get("isError"), f"Tool failed: {result}")
            return result

        try:
            if not modern:
                require_result(client.request(0, "initialize", {"protocolVersion": LEGACY_PROTOCOL,
                    "capabilities": {}, "clientInfo": {"name": "PhysicsVerifier", "version": "1"}}), "initialize")
                client.notify("notifications/initialized")
            call("runtime.play", startPaused=True)
            for frame in range(1, 181):
                call("runtime.step")
                snapshot = read()
                require(snapshot["runtime"]["state"] == "Paused", "Step resumed runtime")
                require(snapshot["publishedFrameIndex"] == frame, "Publication frame")
                require(snapshot["fields"]["tick"] == frame - 1, "Update must observe previous completed tick")
            values = snapshot["fields"]
            require(values["collisions"] >= 1 and abs(values["y"]) < 0.03, f"Blocking: {values}")
            require(values["triggerEnters"] == 1 and values["triggerExits"] == 1, "Trigger lifecycle")
            require(values["bodies"] == 5 and values["rayFloor"], "Deferred destruction or raycast")
            require(values["droppedSeconds"] == 0, "Neutral Steps lost simulation time")
            require(read() == snapshot, "Read advanced physics")
            previous_id = snapshot["runtimeId"]
            call("runtime.stop")
            require(not read()["available"], "Stop retained snapshot")
            call("runtime.play", startPaused=True)
            call("runtime.step")
            restarted = read()
            require(restarted["runtimeId"] != previous_id, "Restart reused runtime identity")
            require(restarted["fields"]["tick"] == 0 and restarted["fields"]["bodies"] == 6, "Restart leaked physics state")
            call("runtime.stop")
            require(scene_path.read_bytes() == original, "Runtime modified authoring file")
        finally:
            client.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--era", choices=("modern", "legacy"), required=True)
    args = parser.parse_args()
    run(args.host.resolve(), args.project.resolve(), args.era == "modern")
    print(f"Physics MCP {args.era}: fixed Step, blocking, triggers, raycast, destruction, restart and isolation passed.")
