"""Prefab authoring and expanded-instance runtime through the production MCP handlers."""
from pathlib import Path
import argparse
import json
import math
import shutil
import tempfile
import sys

sys.dont_write_bytecode = True
from mcp_external_e2e import JanusStdioClient, LEGACY_PROTOCOL, modern_params, require, require_result


def run(host, source, modern):
    with tempfile.TemporaryDirectory(prefix="janus-prefab-e2e-") as directory:
        project = Path(directory) / "Game"
        shutil.copytree(source, project)
        manifest_path = project / "project.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["defaultScene"] = "Scenes/PrefabShowcase.scene"
        manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
        scene_path = project / manifest["defaultScene"]
        before = json.loads(scene_path.read_text(encoding="utf-8"))
        original = scene_path.read_bytes()
        client = JanusStdioClient(host, project)
        request_id = 0

        def request(method, params):
            nonlocal request_id
            request_id += 1
            return client.request(request_id, method, modern_params(**params) if modern else params)

        def call(name, success=True, **arguments):
            result = require_result(request("tools/call", {"name": name, "arguments": arguments}), name)
            require(bool(result.get("isError")) != success, f"Unexpected {name} result: {result}")
            return result["structuredContent"]

        def read(uri):
            result = require_result(request("resources/read", {"uri": uri}), uri)
            return json.loads(result["contents"][0]["text"])

        def initialize():
            if not modern:
                require_result(client.request(0, "initialize", {"protocolVersion": LEGACY_PROTOCOL,
                    "capabilities": {}, "clientInfo": {"name": "PrefabVerifier", "version": "1"}}), "initialize")
                client.notify("notifications/initialized")

        try:
            initialize()
            template = "ab100000-0000-4000-8000-000000000001"
            root = "ab100000-0000-4000-8000-000000000301"
            exported = call("scene.export_prefab", entity=root)["asset"]
            require(scene_path.read_bytes() == original, "Export changed the scene file")
            require(not read("engine://project/info")["authoring"]["dirty"], "Export dirtied authoring")
            token = call("transaction.begin")["transaction"]
            pending = call("scene.instantiate_prefab", asset=template, transaction=token)["entity"]
            require(read("engine://entity/" + pending)["authoringTransaction"]["provisional"], "Missing pending state")
            call("scene.export_prefab", success=False, entity=root)
            call("transaction.rollback", transaction=token)
            require(not read("engine://project/info")["authoring"]["dirty"], "Rollback lost clean state")
            roots = [call("scene.instantiate_prefab", asset=asset)["entity"] for asset in (template, exported)]
            require(len(set(roots + [root, pending])) == 4, "Instance identities collided")
            call("scene.save")
            saved = json.loads(scene_path.read_text(encoding="utf-8"))
            require(len(saved["entities"]) == len(before["entities"]) + 4, "Expected two two-entity subtrees")
            ids = [entity["id"] for entity in saved["entities"]]
            require(len(set(ids)) == len(ids), "Duplicate serialized UUID")
            for instance in roots:
                children = [e for e in saved["entities"] if e["parent"] == instance]
                require(len(children) == 1 and children[0]["name"] == "Badge", "Internal parent remap failed")
                require(children[0]["components"]["SpriteRenderer"]["texture"] ==
                    "ad100000-0000-4000-8000-000000000001", "Asset UUID was remapped")
            saved_bytes = scene_path.read_bytes()
            client.stop()
            client = JanusStdioClient(host, project)
            initialize()
            call("runtime.play", startPaused=True)
            call("scene.instantiate_prefab", success=False, asset=template)
            call("scene.export_prefab", success=False, entity=root)
            for _ in range(60):
                call("runtime.step")
            snapshot = read("engine://runtime/snapshot")
            require(snapshot["available"] and snapshot["fields"]["playing"], "Expanded Animator/Lua did not run")
            require(abs(snapshot["fields"]["elapsed"] - 1) < 1e-5, "Prefab script instance did not advance")
            for instance in roots:
                author = read("engine://entity/" + instance)["components"]["Transform"]["position"]
                runtime = read("engine://runtime/entity/" + instance)["components"]["Transform"]["position"]
                require(abs(runtime["x"] - author["x"] - math.sin(1) * 60) < 0.001,
                    "Remapped script failed or mutated authoring Transform")
            call("runtime.stop")
            require(scene_path.read_bytes() == saved_bytes, "Runtime changed saved instances")
        finally:
            client.stop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--era", choices=("modern", "legacy"), required=True)
    args = parser.parse_args()
    run(args.host.resolve(), args.project.resolve(), args.era == "modern")
    print(f"Prefab MCP {args.era}: export, remap, transaction, persistence, runtime and guards passed.")
