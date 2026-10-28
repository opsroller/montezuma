#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""MCP server for the plant look-dev pipeline. Speaks JSON-RPC on stdin/stdout.

Every tool is one step of tools/assets/lookdev.py, which is where the pipeline
actually lives: step order, the 3840x2160 capture, the .scratch location and the
GPU lock derived from git all belong there, under review, in one language.

No dependencies: the protocol is a handful of JSON-RPC methods over newline
framing, so this needs nothing the flake does not already give it.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
LOOKDEV = "tools/assets/lookdev.py"

STEPS = {
    "params": ("Write tools/assets/themes/plants.json from the species in plants.py. "
               "Must run before atlases, which read it.", []),
    "atlases": ("Draw the 36 leaf and bark atlases at 4096. About 3 minutes when the "
                "colours changed, instant when they did not.", []),
    "build": ("Build the meshes, materials and the catalogue under the flake's pinned "
              "Blender, which is part of the asset: two committed meshes do not "
              "reproduce byte for byte across Blender versions.", ["species"]),
    "pages": ("Write the look-dev scenes: a close-up page per specimen with its empty "
              "companion for masking, plus the seven band rows.", ["species"]),
    "import": ("Run Godot's import pass. REQUIRED after any mesh or texture change: "
               "Godot serves meshes from .godot/imported, so without it a capture "
               "shows the previous assets.", []),
    "check": ("Check the built meshes for degenerate faces, winding against a "
              "neighbour, normals facing the wrong way, UVs straddling an atlas tile, "
              "and orphaned meshes.", []),
    "validate": ("The project's asset rules: paths, budgets, texture channels and "
                 "colour spaces, import settings, provenance and SPDX headers.", []),
    "capture": ("Render look-dev pages at 3840x2160 into .scratch/lookdev. About 15 s "
                "a page, nearly all of it Godot starting up.", ["scope", "species"]),
    "sheet": ("Lay the captured frames out as labelled contact sheets. Nearly every "
              "fault in this library was found by seeing nine species at once.",
              ["per"]),
    "all": ("The whole loop in the order that works: params, atlases, build, pages, "
            "import, check, validate, capture, sheet.", ["scope", "species"]),
}

ARGS = {
    "species": {"type": "string", "description": "comma-separated keys, e.g. heliconia,ceiba"},
    "scope": {"type": "string", "enum": ["species", "rows", "all"]},
    "per": {"type": "number", "description": "frames per contact sheet, default 9"},
}


def schema(names):
    return {"type": "object",
            "properties": {n: ARGS[n] for n in names},
            "additionalProperties": False}


def tool_list():
    return [{"name": step,
             "description": description,
             "inputSchema": schema(names)}
            for step, (description, names) in STEPS.items()]


def run_step(step, arguments):
    """One step, as an argument vector. No shell, so nothing to quote or chain."""
    argv = [sys.executable, LOOKDEV, step]
    for name in ("species", "scope", "per"):
        if arguments.get(name) not in (None, ""):
            argv += [f"--{name}", str(arguments[name])]
    done = subprocess.run(argv, cwd=ROOT, capture_output=True, text=True, check=False)
    text = f"{step}: exit {done.returncode}\n{done.stdout.strip()}"
    if done.stderr.strip():
        text += f"\n--- stderr ---\n{done.stderr.strip()[-2000:]}"
    return {"content": [{"type": "text", "text": text}],
            "isError": done.returncode != 0}


def respond(request_id, result=None, error=None):
    message = {"jsonrpc": "2.0", "id": request_id}
    message["error" if error else "result"] = error or result
    sys.stdout.write(json.dumps(message) + "\n")
    sys.stdout.flush()


def main():
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            request = json.loads(line)
        except json.JSONDecodeError:
            continue
        method, request_id = request.get("method"), request.get("id")
        params = request.get("params") or {}

        if method == "initialize":
            respond(request_id, {
                "protocolVersion": params.get("protocolVersion", "2024-11-05"),
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "montezuma-plants", "version": "0.1.0"},
            })
        elif method == "tools/list":
            respond(request_id, {"tools": tool_list()})
        elif method == "tools/call":
            step = params.get("name")
            if step not in STEPS:
                respond(request_id, error={"code": -32602, "message": f"no step {step}"})
            else:
                respond(request_id, run_step(step, params.get("arguments") or {}))
        elif request_id is not None:
            respond(request_id, error={"code": -32601, "message": f"no method {method}"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
