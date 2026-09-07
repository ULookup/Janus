#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import queue
import shutil
import subprocess
import tempfile
import threading
import time
from pathlib import Path
from typing import Any

MODERN_PROTOCOL = "2026-07-28"
LEGACY_PROTOCOL = "2025-11-25"
PROTOCOL_VERSION_KEY = "io.modelcontextprotocol/protocolVersion"
CLIENT_INFO_KEY = "io.modelcontextprotocol/clientInfo"
CLIENT_CAPABILITIES_KEY = "io.modelcontextprotocol/clientCapabilities"

EXPECTED_TOOLS = {
    "assets.search",
    "scene.create_entity",
    "scene.delete_entity",
    "scene.rename_entity",
    "scene.reparent_entity",
    "scene.add_component",
    "scene.remove_component",
    "scene.set_component_property",
    "scene.save",
    "runtime.play", "runtime.pause", "runtime.stop", "runtime.step",
    "transaction.begin", "transaction.commit", "transaction.rollback",
}

EXPECTED_RESOURCES = {
    "engine://runtime/snapshot",
    "engine://project/info",
    "engine://scene/current",
    "engine://scene/hierarchy",
    "engine://runtime/status", "engine://logs/recent", "engine://profiler/latest-frame",
    "engine://transaction/status", "engine://agent/activity",
}

EXPECTED_TEMPLATES = {
    "engine://entity/{uuid}",
    "engine://asset/{uuid}",
    "engine://runtime/entity/{uuid}",
}


class E2EFailure(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise E2EFailure(message)


class JanusStdioClient:
    def __init__(self, host: Path, project: Path) -> None:
        self._stdout_queue: queue.Queue[str] = queue.Queue()
        self._stderr_lines: list[str] = []
        self._all_stdout_lines: list[str] = []

        self._process = subprocess.Popen(
            [
                str(host),
                "--project",
                str(project),
            ],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
            bufsize=1,
        )

        require(self._process.stdin is not None, "Janus MCP external host stdin pipe unavailable.")
        require(self._process.stdout is not None, "Janus MCP external host stdout pipe unavailable.")
        require(self._process.stderr is not None, "Janus MCP external host stderr pipe unavailable.")

        self._stdout_thread = threading.Thread(
            target=self._collect_stdout,
            name="janus-mcp-e2e-stdout",
            daemon=True,
        )
        self._stderr_thread = threading.Thread(
            target=self._collect_stderr,
            name="janus-mcp-e2e-stderr",
            daemon=True,
        )
        self._stdout_thread.start()
        self._stderr_thread.start()

    def _collect_stdout(self) -> None:
        assert self._process.stdout is not None
        for raw in self._process.stdout:
            line = raw.rstrip("\r\n")
            if line:
                self._all_stdout_lines.append(line)
                self._stdout_queue.put(line)

    def _collect_stderr(self) -> None:
        assert self._process.stderr is not None
        for raw in self._process.stderr:
            self._stderr_lines.append(raw.rstrip("\r\n"))

    def _send(self, message: dict[str, Any]) -> None:
        if self._process.poll() is not None:
            raise E2EFailure(
                "Janus MCP external host exited before request could be sent.\n"
                + self.stderr_text()
            )

        assert self._process.stdin is not None
        self._process.stdin.write(json.dumps(message, separators=(",", ":")) + "\n")
        self._process.stdin.flush()

    def request(
        self,
        request_id: int,
        method: str,
        params: dict[str, Any],
        timeout_seconds: float = 10.0,
    ) -> dict[str, Any]:
        self._send(
            {
                "jsonrpc": "2.0",
                "id": request_id,
                "method": method,
                "params": params,
            }
        )

        try:
            line = self._stdout_queue.get(timeout=timeout_seconds)
        except queue.Empty as exc:
            raise E2EFailure(
                f"Timed out waiting for Janus MCP external host response to {method}.\n"
                + self.stderr_text()
            ) from exc

        try:
            response = json.loads(line)
        except json.JSONDecodeError as exc:
            raise E2EFailure(
                "stdout contamination detected: expected one JSON-RPC message per line, "
                f"got {line!r}.\nDiagnostics:\n{self.stderr_text()}"
            ) from exc

        require(
            response.get("jsonrpc") == "2.0",
            f"Invalid JSON-RPC response: {response!r}",
        )
        require(
            response.get("id") == request_id,
            f"Response id mismatch for {method}: {response!r}",
        )
        return response

    def notify(self, method: str, params: dict[str, Any] | None = None) -> None:
        message: dict[str, Any] = {
            "jsonrpc": "2.0",
            "method": method,
        }
        if params is not None:
            message["params"] = params
        self._send(message)

    def close_stdin(self) -> None:
        if self._process.stdin is not None and not self._process.stdin.closed:
            self._process.stdin.close()

    def stop(self) -> None:
        self.close_stdin()

        # The test fixture exits when stdio reaches EOF. Give it a bounded
        # graceful shutdown window so worker/dispatcher cleanup is exercised.
        try:
            self._process.wait(timeout=5.0)
        except subprocess.TimeoutExpired:
            self._process.terminate()
            try:
                self._process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait(timeout=2.0)

        self._stdout_thread.join(timeout=1.0)
        self._stderr_thread.join(timeout=1.0)

        # Every stdout line must remain protocol JSON, including anything emitted
        # during startup/shutdown that was not consumed as an expected response.
        for line in self._all_stdout_lines:
            try:
                json.loads(line)
            except json.JSONDecodeError as exc:
                raise E2EFailure(
                    "stdout contamination detected after protocol run: "
                    f"{line!r}.\nDiagnostics:\n{self.stderr_text()}"
                ) from exc

    def stderr_text(self) -> str:
        return "\n".join(self._stderr_lines[-120:])


def modern_params(**application_params: Any) -> dict[str, Any]:
    params: dict[str, Any] = dict(application_params)
    params["_meta"] = {
        PROTOCOL_VERSION_KEY: MODERN_PROTOCOL,
        CLIENT_INFO_KEY: {
            "name": "JanusExternalE2E",
            "version": "0.8-test",
        },
        CLIENT_CAPABILITIES_KEY: {},
    }
    return params


def require_result(response: dict[str, Any], method: str) -> dict[str, Any]:
    require(
        "error" not in response,
        f"{method} returned JSON-RPC error: {response!r}",
    )
    result = response.get("result")
    require(isinstance(result, dict), f"{method} returned no object result: {response!r}")
    return result


def read_resource_payload(
    client: JanusStdioClient,
    request_id: int,
    uri: str,
    modern: bool,
) -> dict[str, Any]:
    params: dict[str, Any] = {"uri": uri}
    if modern:
        params = modern_params(**params)

    result = require_result(
        client.request(request_id, "resources/read", params),
        f"resources/read {uri}",
    )
    contents = result.get("contents")
    require(
        isinstance(contents, list) and len(contents) == 1,
        f"Resource {uri} returned invalid contents: {result!r}",
    )
    text = contents[0].get("text")
    require(isinstance(text, str), f"Resource {uri} returned no text payload.")
    payload = json.loads(text)
    require(isinstance(payload, dict), f"Resource {uri} payload is not an object.")
    return payload


def tool_call(
    client: JanusStdioClient,
    request_id: int,
    name: str,
    arguments: dict[str, Any],
) -> dict[str, Any]:
    result = require_result(
        client.request(
            request_id,
            "tools/call",
            modern_params(name=name, arguments=arguments),
        ),
        f"tools/call {name}",
    )
    structured = result.get("structuredContent")
    require(
        isinstance(structured, dict),
        f"Tool {name} returned no structuredContent: {result!r}",
    )
    require(
        structured.get("ok") is True,
        f"Tool {name} failed: {result!r}",
    )
    return structured


def verify_catalogs(client: JanusStdioClient, first_id: int, modern: bool) -> int:
    request_id = first_id

    def params() -> dict[str, Any]:
        return modern_params() if modern else {}

    tools = require_result(
        client.request(request_id, "tools/list", params()),
        "tools/list",
    )
    request_id += 1
    tool_names = {entry.get("name") for entry in tools.get("tools", [])}
    require(EXPECTED_TOOLS.issubset(tool_names), f"Missing MCP tools: {EXPECTED_TOOLS - tool_names}")

    resources = require_result(
        client.request(request_id, "resources/list", params()),
        "resources/list",
    )
    request_id += 1
    resource_uris = {entry.get("uri") for entry in resources.get("resources", [])}
    require(
        EXPECTED_RESOURCES.issubset(resource_uris),
        f"Missing MCP resources: {EXPECTED_RESOURCES - resource_uris}",
    )

    templates = require_result(
        client.request(request_id, "resources/templates/list", params()),
        "resources/templates/list",
    )
    request_id += 1
    template_uris = {
        entry.get("uriTemplate")
        for entry in templates.get("resourceTemplates", [])
    }
    require(
        EXPECTED_TEMPLATES.issubset(template_uris),
        f"Missing MCP resource templates: {EXPECTED_TEMPLATES - template_uris}",
    )

    return request_id


def verify_debug(client: JanusStdioClient, project: Path, modern: bool) -> None:
    request_id = 1000
    def call(name: str, arguments: dict[str, Any], success: bool = True) -> dict[str, Any]:
        nonlocal request_id
        request_id += 1
        params = dict(name=name, arguments=arguments)
        result = require_result(client.request(request_id, "tools/call", modern_params(**params) if modern else params), name)
        require(result.get("isError", False) is not success, f"Unexpected tool outcome: {result}")
        return result["structuredContent"]
    def read(uri: str) -> dict[str, Any]:
        nonlocal request_id
        request_id += 1
        return read_resource_payload(client, request_id, uri, modern)
    found = call("assets.search", {"name": "JanusPixel", "type": "font", "limit": 1})
    require(found["total"] == 1 and len(found["assets"]) == 1, f"Font discovery failed: {found}")
    font = found["assets"][0]["handle"]
    require(found["assets"][0]["path"] == "Fonts/JanusPixel.font.json", "Unstable asset path")
    token = call("transaction.begin", {"label": "E2E authoring group"})["transaction"]
    require(read("engine://runtime/status")["authoringReadOnly"], "Transaction lock missing from runtime status")
    created = call("scene.create_entity", {"name": "Transactional", "transaction": token})["entity"]
    call("scene.add_component", {"entity": created, "component": "Camera", "transaction": token})
    call("scene.set_component_property", {"entity": created, "component": "Transform", "property": "position", "value": {"x": 4.0, "y": 5.0}, "transaction": token})
    require(read("engine://entity/" + created)["authoringTransaction"]["provisional"], "Pending state not marked")
    call("transaction.commit", {"transaction": "99999999-9999-4999-8999-999999999999"}, success=False)
    require(read("engine://transaction/status")["state"] == "Active", "Foreign token aborted owner")
    call("transaction.commit", {"transaction": token})
    call("transaction.commit", {"transaction": token})
    require(read("engine://transaction/status")["state"] == "Idle", "Commit left transaction active")
    activity = read("engine://agent/activity?after=0&limit=200")["entries"]
    committed = [row for row in activity if row["transaction"] == token and row["outcome"] == "Committed" and row["commandId"] != "0"]
    filtered = read("engine://agent/activity?after=0&transactionId=" + token)["entries"]
    require(all(row["transaction"] == token for row in filtered), "Activity transaction filter failed")
    require(len(committed) == 3 and all(row["effects"] for row in committed), f"Missing command receipts: {committed}")
    token = call("transaction.begin", {})["transaction"]
    call("scene.rename_entity", {"entity": created, "name": "Rollback", "transaction": token})
    call("scene.delete_entity", {"entity": "99999999-9999-4999-8999-999999999999", "transaction": token}, success=False)
    require(read("engine://transaction/status")["state"] == "Idle", "Failed mutation did not auto-abort")
    require(read("engine://entity/" + created)["name"] == "Transactional", "Rollback failed to restore name")
    token = call("transaction.begin", {"label": "UI authoring rollback"})["transaction"]
    ui_parent = call("scene.create_entity", {"name": "UIParent", "transaction": token})["entity"]
    call("scene.add_component", {"entity": ui_parent, "component": "Canvas", "transaction": token})
    call("scene.add_component", {"entity": created, "component": "UIRect", "transaction": token})
    call("scene.add_component", {"entity": created, "component": "Panel", "transaction": token})
    call("scene.set_component_property", {"entity": created, "component": "UIRect", "property": "offset", "value": {"x": 24.0, "y": 48.0}, "transaction": token})
    call("scene.reparent_entity", {"entity": created, "parent": ui_parent, "siblingIndex": 0, "transaction": token})
    call("scene.add_component", {"entity": created, "component": "Text", "transaction": token})
    call("scene.set_component_property", {"entity": created, "component": "Text", "property": "font", "value": font, "transaction": token})
    call("scene.set_component_property", {"entity": created, "component": "Text", "property": "content", "value": "HP 12\nREADY", "transaction": token})
    call("scene.add_component", {"entity": created, "component": "Button", "transaction": token})
    call("scene.set_component_property", {"entity": created, "component": "Button", "property": "interactable", "value": False, "transaction": token})
    ui_entity = read("engine://entity/" + created)
    require(ui_entity["components"]["Button"]["interactable"] is False, "Button mutation missing")
    require(ui_entity["components"]["Text"]["content"] == "HP 12\nREADY", "Text mutation missing")
    require(ui_entity["parent"] == ui_parent, "Reparent did not reach shared authoring Scene")
    require(ui_entity["components"]["UIRect"]["offset"] == {"x": 24.0, "y": 48.0}, "UI reflection mutation missing")
    call("transaction.rollback", {"transaction": token})
    ui_entity = read("engine://entity/" + created)
    require(ui_entity["parent"] is None and "UIRect" not in ui_entity["components"] and "Text" not in ui_entity["components"] and "Button" not in ui_entity["components"], "UI transaction rollback did not restore root and components")
    player = "44444444-4444-4444-8444-444444444444"
    call("scene.set_component_property", dict(entity=player, component="Transform", property="position", value={"x": 10.0, "y": 0.0}))
    script = project / "Scripts" / "PlayerController.lua"
    script.write_text("return { OnUpdate = function(self, dt) local x,y = self.entity:get_position(); self.entity:set_position(x + 60 * dt,y) end }", encoding="utf-8")
    started = call("runtime.play", {"startPaused": True})
    first_id = started["runtime"]["runtimeId"]
    call("runtime.play", {"startPaused": False}, success=False)
    for _ in range(3): call("runtime.step", {})
    state = read("engine://runtime/status")
    require(state["state"] == "Paused" and state["frameIndex"] == 3, f"Wrong paused frame: {state}")
    profile = read("engine://profiler/latest-frame")
    require(profile["available"] and profile["simulationFrame"] == 3 and profile["scopes"], f"Missing real runtime profile: {profile}")
    historical = read("engine://profiler/latest-frame?frameId=" + str(profile["frameId"]))
    require(historical["frameId"] == profile["frameId"], "Historical frame not stable")
    require(not read("engine://profiler/latest-frame?frameId=999999999")["available"], "Missing frame claimed available")
    entity = read("engine://runtime/entity/" + player)
    require(entity["components"]["Transform"]["position"] == {"x": 13.0, "y": 0.0}, f"Runtime did not execute three Lua steps: {entity}")
    require(read("engine://entity/" + player)["components"]["Transform"]["position"] == {"x": 10.0, "y": 0.0}, "Runtime polluted authoring")
    call("runtime.stop", {})
    script.write_text('return { OnUpdate = function(self, dt) error("v09 deliberate fault") end }', encoding="utf-8")
    call("runtime.play", {"startPaused": True})
    call("runtime.step", {}, success=False)
    state = read("engine://runtime/status")
    require(state["state"] == "Faulted" and state["partialUpdate"] and state["runtimeId"] != first_id, "Fault snapshot lost")
    logs = read("engine://logs/recent?level=Error&limit=10")
    require(any("v09 deliberate fault" in row["message"] for row in logs["entries"]), f"Runtime error missing: {logs}")
    call("scene.rename_entity", dict(entity=player, name="Forbidden"), success=False)
    call("runtime.stop", {})
    token = call("transaction.begin", {})["transaction"]
    call("scene.rename_entity", {"entity": created, "name": "Disconnect", "transaction": token})
    # Closing stdin below exercises production main-thread disconnect rollback.


def run_modern(host: Path, source_project: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="janus-mcp-modern-") as temp_root:
        project = Path(temp_root) / "SandboxProject"
        shutil.copytree(source_project, project)

        client = JanusStdioClient(host, project)
        try:
            discover = require_result(
                client.request(
                    1,
                    "server/discover",
                    modern_params(),
                    timeout_seconds=10.0,
                ),
                "server/discover",
            )
            supported = discover.get("supportedVersions", [])
            require(
                MODERN_PROTOCOL in supported,
                f"Modern protocol {MODERN_PROTOCOL} not advertised: {discover!r}",
            )

            request_id = verify_catalogs(client, 2, modern=True)

            project_info = read_resource_payload(
                client,
                request_id,
                "engine://project/info",
                modern=True,
            )
            request_id += 1
            require(
                project_info.get("project", {}).get("displayPath") == "SandboxProject",
                f"Unexpected project metadata: {project_info!r}",
            )
            require(
                project_info.get("authoring", {}).get("readOnly") is False,
                f"Editor unexpectedly read-only: {project_info!r}",
            )

            scene_before = read_resource_payload(
                client,
                request_id,
                "engine://scene/current",
                modern=True,
            )
            request_id += 1
            before_count = scene_before.get("entityCount")
            require(isinstance(before_count, int), f"Invalid scene entity count: {scene_before!r}")

            hierarchy_before = read_resource_payload(
                client,
                request_id,
                "engine://scene/hierarchy",
                modern=True,
            )
            request_id += 1
            initial_entities = hierarchy_before.get("entities", [])
            require(initial_entities, f"Initial hierarchy is empty: {hierarchy_before!r}")
            initial_uuid = initial_entities[0].get("uuid")
            require(isinstance(initial_uuid, str), f"Hierarchy UUID missing: {hierarchy_before!r}")

            initial_entity = read_resource_payload(
                client,
                request_id,
                f"engine://entity/{initial_uuid}",
                modern=True,
            )
            request_id += 1
            require(
                initial_entity.get("uuid") == initial_uuid,
                f"Entity resource identity mismatch: {initial_entity!r}",
            )

            created = tool_call(
                client,
                request_id,
                "scene.create_entity",
                {"name": "ExternalAgentEntity"},
            )
            request_id += 1
            created_uuid = created.get("entity")
            require(isinstance(created_uuid, str), f"Create tool returned no UUID: {created!r}")

            tool_call(
                client,
                request_id,
                "scene.add_component",
                {
                    "entity": created_uuid,
                    "component": "Camera",
                },
            )
            request_id += 1

            tool_call(
                client,
                request_id,
                "scene.set_component_property",
                {
                    "entity": created_uuid,
                    "component": "Transform",
                    "property": "position",
                    "value": {
                        "x": 12.5,
                        "y": -4.25,
                    },
                },
            )
            request_id += 1

            tool_call(
                client,
                request_id,
                "scene.rename_entity",
                {
                    "entity": created_uuid,
                    "name": "ExternalAgentRenamed",
                },
            )
            request_id += 1

            tool_call(
                client,
                request_id,
                "scene.save",
                {},
            )
            request_id += 1

            entity_after = read_resource_payload(
                client,
                request_id,
                f"engine://entity/{created_uuid}",
                modern=True,
            )
            request_id += 1
            require(
                entity_after.get("name") == "ExternalAgentRenamed",
                f"Rename not visible through entity resource: {entity_after!r}",
            )
            transform = entity_after.get("components", {}).get("Transform", {})
            require(
                transform.get("position") == {"x": 12.5, "y": -4.25},
                f"Transform mutation not visible through entity resource: {entity_after!r}",
            )
            require(
                "Camera" in entity_after.get("components", {}),
                f"Added Camera not visible through entity resource: {entity_after!r}",
            )

            scene_after = read_resource_payload(
                client,
                request_id,
                "engine://scene/current",
                modern=True,
            )
            request_id += 1
            require(
                scene_after.get("entityCount") == before_count + 1,
                f"Scene entity count did not advance: before={before_count}, after={scene_after!r}",
            )

            hierarchy_after = read_resource_payload(
                client,
                request_id,
                "engine://scene/hierarchy",
                modern=True,
            )
            require(
                any(
                    entity.get("uuid") == created_uuid
                    and entity.get("name") == "ExternalAgentRenamed"
                    for entity in hierarchy_after.get("entities", [])
                ),
                f"Created entity not visible in hierarchy: {hierarchy_after!r}",
            )

            scene_path = project / "Scenes" / "Battle.scene"
            persisted = json.loads(scene_path.read_text(encoding="utf-8"))
            persisted_entity = next(
                (
                    entity
                    for entity in persisted.get("entities", [])
                    if entity.get("id") == created_uuid
                ),
                None,
            )
            require(
                isinstance(persisted_entity, dict),
                f"Saved Scene does not contain created entity {created_uuid}.",
            )
            require(
                persisted_entity.get("name") == "ExternalAgentRenamed",
                f"Saved Scene rename mismatch: {persisted_entity!r}",
            )
            require(
                persisted_entity.get("components", {})
                .get("Transform", {})
                .get("position")
                == [12.5, -4.25],
                f"Saved Scene Transform mismatch: {persisted_entity!r}",
            )
            require(
                "Camera" in persisted_entity.get("components", {}),
                f"Saved Scene Camera missing: {persisted_entity!r}",
            )
            verify_debug(client, project, modern=True)
        finally:
            client.stop()


def run_legacy(host: Path, source_project: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="janus-mcp-legacy-") as temp_root:
        project = Path(temp_root) / "SandboxProject"
        shutil.copytree(source_project, project)

        client = JanusStdioClient(host, project)
        try:
            initialized = require_result(
                client.request(
                    1,
                    "initialize",
                    {
                        "protocolVersion": LEGACY_PROTOCOL,
                        "capabilities": {},
                        "clientInfo": {
                            "name": "JanusLegacyExternalE2E",
                            "version": "0.8-test",
                        },
                    },
                    timeout_seconds=10.0,
                ),
                "initialize",
            )
            require(
                initialized.get("protocolVersion") == LEGACY_PROTOCOL,
                f"Legacy protocol counter-offer mismatch: {initialized!r}",
            )

            client.notify("notifications/initialized")

            next_id = verify_catalogs(client, 2, modern=False)

            project_info = read_resource_payload(
                client,
                next_id,
                "engine://project/info",
                modern=False,
            )
            require(
                project_info.get("project", {}).get("displayPath") == "SandboxProject",
                f"Legacy project resource mismatch: {project_info!r}",
            )
            verify_debug(client, project, modern=False)
        finally:
            client.stop()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True, type=Path)
    parser.add_argument("--project", required=True, type=Path)
    parser.add_argument("--era", required=True, choices=("modern", "legacy"))
    args = parser.parse_args()

    host = args.host.resolve()
    source_project = args.project.resolve()

    require(host.is_file(), f"Janus MCP external host executable not found: {host}")
    require(source_project.is_dir(), f"SandboxProject not found: {source_project}")

    started = time.monotonic()
    if args.era == "modern":
        run_modern(host, source_project)
    else:
        run_legacy(host, source_project)

    elapsed = time.monotonic() - started
    print(
        f"Janus external MCP {args.era} E2E passed in {elapsed:.2f}s.",
        file=__import__("sys").stderr,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except E2EFailure as error:
        print(f"E2E failure: {error}", file=__import__("sys").stderr)
        raise SystemExit(1)
