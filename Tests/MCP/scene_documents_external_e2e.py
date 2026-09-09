"""Scene lifecycle through the existing production handlers and either stdio era."""
from pathlib import Path
import argparse
import json
import shutil
import sys
import tempfile

sys.dont_write_bytecode = True
from mcp_external_e2e import JanusStdioClient, LEGACY_PROTOCOL, modern_params, require, require_result


def run(host, source, modern, native_editor):
    with tempfile.TemporaryDirectory(prefix="janus-documents-") as directory:
        project = Path(directory) / "Project"
        shutil.copytree(source, project)
        manifest = (project / "project.json").read_bytes()
        original_path = json.loads(manifest)["defaultScene"]
        original = (project / original_path).read_bytes()
        client = JanusStdioClient(host, project, native_editor=native_editor)
        request_id = 0

        def request(method, params):
            nonlocal request_id
            request_id += 1
            return client.request(request_id, method, modern_params(**params) if modern else params)

        def call(tool_name, success=True, **args):
            result = require_result(request("tools/call", {"name": tool_name, "arguments": args}), tool_name)
            require(bool(result.get("isError")) != success, f"Unexpected {tool_name}: {result}")
            return result["structuredContent"]

        def read(uri="engine://scene/current"):
            result = require_result(request("resources/read", {"uri": uri}), uri)
            return json.loads(result["contents"][0]["text"])

        try:
            if not modern:
                require_result(client.request(0, "initialize", {"protocolVersion": LEGACY_PROTOCOL,
                    "capabilities": {}, "clientInfo": {"name": "SceneDocuments", "version": "1"}}), "initialize")
                client.notify("notifications/initialized")
            before = read()
            call("scene.open", success=False, path="../escape.scene")
            (project / "Scenes/Bad.scene").write_text("{invalid", encoding="utf-8")
            call("scene.open", success=False, path="Scenes/Bad.scene")
            require(read() == before, "Failed open changed context")
            call("scene.new", path="Scenes/新场景.scene", expectedRevision=before["sceneRevision"])
            current = read()
            require(current["entityCount"] == 0 and not current["hasSavedFile"], "New scene state")
            require(current["sceneRevision"] == before["sceneRevision"] + 1, "Revision did not advance")
            require(current["bindingEpoch"] > before["bindingEpoch"], "Epoch did not advance")
            require(not (project / "Scenes/新场景.scene").exists(), "New wrote a file")
            entity = call("scene.create_entity", name="Persistent E1 entity")["entity"]
            call("scene.open", success=False, path=original_path)
            (project / "Scenes/新场景.scene").write_text("occupied", encoding="utf-8")
            call("scene.save", success=False)
            require((project / "Scenes/新场景.scene").read_text() == "occupied", "First save overwrote collision")
            call("scene.save_as", path="Scenes/另存.scene")
            require(read()["hasSavedFile"], "SaveAs not published")
            call("scene.save_as", success=False, path=original_path)
            call("scene.open", path=original_path)
            call("scene.open", path="Scenes/另存.scene")
            require(read("engine://entity/" + entity)["name"] == "Persistent E1 entity", "Reopen lost UUID/data")
            token = call("transaction.begin")["transaction"]
            call("scene.create_entity", name="Pending", transaction=token)
            for name in ("scene.new", "scene.open", "scene.save_as"):
                result = request("tools/call", {"name": name, "arguments": {"path": original_path, "transaction": token}})
                require("error" in result, "Lifecycle accepted transaction parameter")
                require(read("engine://transaction/status")["state"] == "Active", "Lifecycle aborted transaction")
            call("transaction.rollback", transaction=token)
            call("scene.save_as", path="Scenes/新场景.scene", overwrite=True)
            require((project / "Scenes/新场景.scene").read_text(encoding="utf-8") != "occupied", "Explicit overwrite failed")
            require((project / "project.json").read_bytes() == manifest, "Scene switch changed defaultScene")
            require((project / original_path).read_bytes() == original, "Original scene changed")
            require(not read("engine://project/info")["authoring"]["dirty"], "Final document dirty")
        finally:
            client.stop()
    print(f"Scene documents {'modern' if modern else 'legacy'}: new, guarded open, save collision, UTF-8 SaveAs, reopen, UUID and transaction isolation passed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--era", choices=("modern", "legacy"), required=True)
    parser.add_argument("--native-editor", action="store_true")
    args = parser.parse_args()
    run(args.host.resolve(), args.project.resolve(), args.era == "modern", args.native_editor)
