# AGENTS.md

## Purpose

This repository contains an unofficial Unreal Engine MCP fork. Coding agents must treat it as a mixed Python, Rust, and Unreal C++ project with two different execution environments:

- **Local Windows development** can run Unreal Editor, build the plugin against the installed engine, and perform live MCP bridge tests.
- **Cloud coding agents such as Jules** run in an ephemeral Ubuntu VM. They can edit source, run Python and Rust tests, and perform static C++ checks, but they cannot launch the local Windows Unreal Editor or rely on `C:\...` paths.

If code, docs, and this file disagree, verify the current repository state from source and tests before editing.

## Source Of Truth

- MCP server entrypoint: `Python/unreal_mcp_server_advanced.py`
- Python tool modules: `Python/server/*_tools.py`
- Python connection, framing, and FastMCP setup: `Python/server/core.py`
- Python validation helpers: `Python/server/validation.py`
- Unreal plugin source currently present in this checkout: `Plugins/UnrealMCP/Source/UnrealMCP/`
- Rust scene sync service: `rust/scene-syncd/`
- Feature backlog and coverage checklist: `docs/superpowers/plans/tasks.md`
- Jules implementation slice guide: `docs/superpowers/plans/jules-implementation-brief.md`
- Public tool docs: `README.md` and `Guides/tools-reference.md`
- Change history: `CHANGELOG.md`

Do not recreate a second Unreal plugin source tree. If another project directory contains only `Intermediate/`, `Binaries/`, or generated files, do not treat it as the canonical plugin source.

## General Rules

- Read relevant code, tests, and docs before editing.
- Prefer small, reviewable changes scoped to one feature area.
- Keep code, tests, docs, and `CHANGELOG.md` in sync for meaningful behavior changes.
- Do not silently change public MCP command names, parameter names, response shapes, socket framing, retry behavior, or scene-sync API contracts.
- Source-code comments and docstrings must be in English.
- Do not hardcode secrets, API keys, local absolute paths, or machine-specific Unreal install paths.
- Do not claim a UE-dependent feature is fully verified unless Unreal Editor or an Unreal build actually ran in an environment that has Unreal installed.

## Jules Cloud Workflow

Jules is useful for asynchronous implementation branches, but tasks must be shaped for its Ubuntu VM constraints.

When running in Jules:

- Use the setup script in `scripts/jules-setup.sh` as the Initial Setup command.
- Assume no Unreal Engine installation and no Windows filesystem paths.
- Do not run `UnrealEditor.exe`, `Build.bat`, `.uproject` editor launches, or commands that depend on `C:\Program Files\Epic Games\...`.
- Do not mark live MCP bridge behavior, editor startup, C++ plugin compilation, or asset creation inside Unreal as fully verified.
- For UE-facing C++ changes, compile-level verification must be deferred to local Windows unless a task explicitly provides a Linux Unreal Engine environment.
- Still add or update Python/Rust contract tests and static mapping tests whenever the change affects tool contracts.
- End each task with a clear "Local Windows verification required" note when Unreal runtime/editor coverage remains.

Recommended Jules branch shape:

- Use one branch per backlog slice, for example `jules/umg-widget-tools`, `jules/asset-management-tools`, or `jules/datatable-tools`.
- Keep branches independent. Avoid multiple Jules tasks editing the same files at the same time, especially `Python/server/scene_tools.py`, bridge dispatch files, and shared docs.
- Prefer implementing one MCP tool group at a time: Python facade, C++ command handler if needed, unit/contract tests, docs, changelog.

Good Jules task prompt pattern:

```text
Read AGENTS.md, docs/superpowers/plans/tasks.md, and docs/superpowers/plans/jules-implementation-brief.md. Implement only the <feature area> slice.
Respect the Jules cloud constraints: do not attempt to launch Unreal Editor.
Add/adjust Python tool mapping tests and docs. If C++ changes are needed, keep them minimal and note that local Windows UE build verification is required.
Run scripts/jules-setup.sh, then run the narrow relevant pytest/cargo tests.
```

## Required Checks

For Python-side tool changes:

```bash
cd Python
python -m pytest tests/unit/test_tool_registration_and_mapping.py -v
python -m pytest tests/unit tests/contract -v
```

For scene-sync Rust changes:

```bash
cd rust/scene-syncd
cargo test --locked
```

For docs-only changes:

```bash
cd Python
python -m pytest tests/unit/test_docs_consistency.py -v
```

For Unreal C++ plugin changes on local Windows with Unreal installed:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' FlopperamUnrealMCPEditor Win64 Development -Project='C:\development\unreal-engine-mcp\FlopperamUnrealMCP\FlopperamUnrealMCP.uproject' -WaitMutex -NoHotReload
```

If the project path or engine path differs, record the exact path used in the verification notes.

## Tool Contract Rules

- Every public MCP tool must use `@mcp.tool()` explicitly.
- If adding a Python MCP tool that sends a C++ command, update `TestPythonToCppCommandMapping.known_mapping` in `Python/tests/unit/test_tool_registration_and_mapping.py`.
- If adding a C++ command without a Python MCP facade, either add the facade or document and whitelist the intentional gap in the drift test.
- Preserve canonical request field names across Python, C++, docs, and tests.
- Material and Blueprint graph JSON tools must be tested for both command routing and JSON-to-command expansion.

## Backlog Guidance For `docs/superpowers/plans/tasks.md`

The checklist is broad. Treat unchecked items as backlog candidates, not as proof that a feature is required in one branch.

Best next Jules-friendly slices:

1. **Content Browser / Asset Management**: folder operations, asset listing/search, rename/move/delete wrappers, docs, and tests. Mostly editor API design plus contract tests; local UE verification required for runtime behavior.
2. **Data Tables / Data Assets**: CSV/JSON DataTable creation and export contracts. Strong LLM value and easy to test at the Python contract layer.
3. **UMG / UI**: Widget Blueprint creation and basic widget tree JSON. High product value, but UE verification is mandatory.
4. **Project / Editor Control**: read-only project settings, dirty asset listing, save operations, editor logs. Useful operational base, but be careful with destructive editor commands.
5. **Testing / Validation**: commands that compile Blueprints, validate missing references, and produce structured reports. Good support layer for later autonomous work.

Avoid starting with large runtime-only areas in Jules unless the task is limited to scaffolding and tests: Landscape, Niagara, Animation, Sequencer, Movie Render Queue, platform packaging, GAS, and multiplayer runtime validation.

## Manual Verification Notes

When automation cannot run in the current environment, include:

- environment used
- exact commands run
- exact MCP tool inputs if applicable
- expected result
- actual observed result
- remaining verification gap

Example:

```text
Local Windows verification required: Jules could not launch Unreal Editor. On a Windows machine with UE 5.7, build the plugin, launch the canonical project, call create_widget_blueprint with the sample JSON payload, and confirm the Widget Blueprint is created and compiles.
```
