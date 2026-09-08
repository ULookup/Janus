"""Agent acceptance: discover/configure a prefab, reopen, run shared game rules and inspect."""
from pathlib import Path
import argparse
import json
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
from mcp_external_e2e import JanusStdioClient, LEGACY_PROTOCOL, modern_params, require, require_result


def run(host, source, modern, native_editor=False):
    with tempfile.TemporaryDirectory(prefix="janus-integrated-") as directory:
        project = Path(directory) / "Game"
        shutil.copytree(source, project)
        manifest = json.loads((project / "project.json").read_text(encoding="utf-8"))
        manifest["defaultScene"] = "Scenes/IntegratedVerification.scene"
        (project / "project.json").write_text(json.dumps(manifest), encoding="utf-8")
        scene_path = project / manifest["defaultScene"]
        original = scene_path.read_bytes()
        client = JanusStdioClient(host, project, native_editor=native_editor)
        request_id = 0

        def request(method, params):
            nonlocal request_id
            request_id += 1
            return client.request(request_id, method, modern_params(**params) if modern else params)

        def call(tool_name, success=True, **arguments):
            result = require_result(request("tools/call", {"name": tool_name, "arguments": arguments}), tool_name)
            require(bool(result.get("isError")) != success, f"Unexpected {tool_name}: {result}")
            return result["structuredContent"]

        def read(uri):
            result = require_result(request("resources/read", {"uri": uri}), uri)
            return json.loads(result["contents"][0]["text"])

        def initialize():
            if not modern:
                require_result(client.request(0, "initialize", {"protocolVersion": LEGACY_PROTOCOL,
                    "capabilities": {}, "clientInfo": {"name": "IntegratedVerifier", "version": "1"}}), "initialize")
                client.notify("notifications/initialized")

        try:
            initialize()
            assets = call("assets.search", name="Fighter", type="prefab")["assets"]
            require(len(assets) == 1, "Fighter template discovery")
            template = assets[0]["handle"]
            # A failed command must roll back both the instance and pending history.
            token = call("transaction.begin")["transaction"]
            probe = call("scene.instantiate_prefab", asset=template, transaction=token)["entity"]
            duplicate = call("scene.duplicate_entity", entity=probe, transaction=token)["entity"]
            require(duplicate != probe, "Duplicate reused root UUID")
            source_entity = read("engine://entity/" + probe)
            copy_entity = read("engine://entity/" + duplicate)
            require(copy_entity["name"] == source_entity["name"] + " Copy", "Duplicate root name")
            require(copy_entity["components"] == source_entity["components"], "Duplicate changed component or asset values")
            call("scene.delete_entity", success=False,
                 entity="99999999-9999-4999-8999-999999999999", transaction=token)
            require(read("engine://transaction/status")["state"] == "Idle", "Failure did not abort")
            require(not read("engine://project/info")["authoring"]["dirty"], "Rollback dirty state")
            require(scene_path.read_bytes() == original, "Rollback changed file")
            token = call("transaction.begin")["transaction"]
            actor = call("scene.instantiate_prefab", asset=template, transaction=token)["entity"]
            call("scene.rename_entity", entity=actor, name="Spectator", transaction=token)
            call("scene.set_component_property", entity=actor, component="Transform",
                 property="position", value={"x": 6.0, "y": 2.0}, transaction=token)
            call("transaction.commit", transaction=token)
            call("scene.save")
            call("scene.export_prefab", success=False, entity=actor, name="../invalid")
            exported = call("scene.export_prefab", entity=actor, name="Named Spectator")["asset"]
            found = call("assets.search", name="Named Spectator-", type="prefab")["assets"]
            require(len(found) == 1 and found[0]["handle"] == exported, "Named Prefab not discoverable")
            require(found[0]["path"] == "Prefabs/Named Spectator-" + exported + ".prefab", "Named Prefab path")
            require(not read("engine://project/info")["authoring"]["dirty"], "Export dirtied scene")
            saved = scene_path.read_bytes()
            client.stop()
            client = JanusStdioClient(host, project, native_editor=native_editor)
            initialize()
            require(read("engine://entity/" + actor)["components"]["Transform"]["position"]["y"] == 2,
                    "Configured instance did not survive reopen")
            require(call("assets.search", name="Named Spectator-", type="prefab")["assets"][0]["handle"] == exported,
                    "Named Prefab registry did not survive reopen")
            call("runtime.play", startPaused=True)
            call("scene.duplicate_entity", success=False, entity=actor)
            call("scene.instantiate_prefab", success=False, asset=template)
            for frame in range(1, 481):
                call("runtime.step")
                if frame in (90, 181, 421, 480):
                    snap = read("engine://runtime/snapshot")
                    fields = snap["fields"]
                    require(snap["runtime"]["state"] == "Paused", "Step resumed runtime")
                    require(snap["publishedFrameIndex"] == frame, "Stale snapshot")
                    require(fields["bodies"] == 4 and fields["landings"] >= 2, "Missing prefab physics")
                    require(not fields["audioAvailable"], "Start-paused opened output")
                    require(fields["droppedSeconds"] == 0, "Neutral step dropped simulation time")
                    if frame == 181:
                        require(fields["phase"] == "victory" and fields["hits"] == 5, "Victory feedback")
                    if frame >= 421:
                        require(fields["phase"] == "defeat" and fields["verifiedVictory"] and
                                fields["verifiedDefeat"] and fields["hits"] == 3, "Both outcomes")
            runtime_y = read("engine://runtime/entity/" + actor)["components"]["Transform"]["position"]["y"]
            require(abs(runtime_y - 2) > 0.1, "Instantiated body did not simulate")
            require(read("engine://entity/" + actor)["components"]["Transform"]["position"]["y"] == 2,
                    "Runtime mutated authoring")
            profile = read("engine://profiler/latest-frame")
            required = {"Runtime.UI", "Runtime.Lua", "Runtime.Physics", "Runtime.Animation", "Runtime.Audio"}
            names = {scope["name"] for scope in profile.get("scopes", [])}
            # A native Editor continues rendering while paused. Its newest frame can be render-only.
            # Read bounded retained frames, without advancing gameplay, to find the completed Step.
            latest_id = profile.get("frameId", 0)
            for offset in range(1, min(latest_id, 120)):
                if required <= names:
                    break
                profile = read(f"engine://profiler/latest-frame?frameId={latest_id - offset}")
                names = {scope["name"] for scope in profile.get("scopes", [])}
            require(required <= names,
                    "Shared stage diagnostics missing")
            require(not read("engine://logs/recent?level=Error")["entries"], "Unexpected runtime errors")
            call("runtime.stop")
            require(scene_path.read_bytes() == saved, "Simulation changed saved scene")
            require(not read("engine://project/info")["authoring"]["dirty"], "Simulation dirtied scene")
        finally:
            client.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--era", choices=("modern", "legacy"), required=True)
    parser.add_argument("--native-editor", action="store_true")
    args = parser.parse_args()
    run(args.host.resolve(), args.project.resolve(), args.era == "modern", args.native_editor)
    print(f"Integrated MCP {args.era}: authoring, rollback, reopen, 480 steps, both outcomes and diagnostics passed.")
