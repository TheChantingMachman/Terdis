# Claude Code — Terdis Project Instructions

## What this project is

Terdis is a **Rust reimplementation of Redis**, built with the Dark Factory agentic TDD
pipeline. `spec/` is human-owned. `src/` and `tests/` are pipeline-owned.

## Role boundaries

This repo is for **AI pair programming**: spec authoring, reviewing pipeline output, and
iterating on the specification. Pipeline tooling bugs and specdb development happen in
separate repos — do not attempt to debug or modify pipeline infrastructure from here.

## Scale context

Prior pipeline projects hit context overload at ~800 LOC / ~900 lines spec / ~1100 lines
tests. The specdb CLI exists specifically to prevent this. As the spec grows:
- Always prefer CLI queries over reading spec.yaml directly
- Use `--format brief` for orientation, then `--id` for targeted reads
- Flag any usability issues with the CLI — feedback directly improves the pipeline

## Spec management

The authoritative spec lives at `spec/spec.yaml`. **Do not read this file directly** — as
the spec grows it will overwhelm your context window. Use the bundled specdb CLI instead:

```bash
# Orient
python3 spec/specdb.py sections
python3 spec/specdb.py tags

# Browse
python3 spec/specdb.py query --format brief
python3 spec/specdb.py query --status draft --format brief

# Drill in
python3 spec/specdb.py query --id commands.set
python3 spec/specdb.py query --section Commands --format yaml
python3 spec/specdb.py query --tags "core,write" --format yaml

# Add a new entry
python3 spec/specdb.py add --id commands.ping --section Commands \
  --description "PING returns PONG. Takes no arguments." \
  --tags "commands,ping,core" --status draft

# Update an existing entry
python3 spec/specdb.py update --id commands.set --status active
```

Only read `spec/spec.yaml` directly if specdb is broken or you need to fix YAML syntax errors.

## Spec entry deletion policy

**Never hard-delete spec entries.** The pipeline uses TDD — deleting an entry orphans its
tests, breaking stale detection and preventing proper test refactoring.

To retire a spec entry, deprecate it:
```bash
python3 spec/specdb.py update --id some.entry --status deprecated
```

Deprecated entries are ignored by pipeline agents (they query by `--status draft` or
`--status active`) but remain visible to `specdb stale` and `specdb diff`, allowing tests
to be properly flagged and cleaned up. The full lifecycle is:
`draft → active → implemented → deprecated`

Hard pruning of deprecated entries requires human review to confirm all dependent tests
and entries have been updated first.

## Ownership boundaries

| Directory | Owner | Notes |
|---|---|---|
| `spec/` | Human | Never modified by pipeline agents |
| `spec/specdb.py` | Human (vendored) | Update by copying from SpecDB repo after a build |
| `src/` | Pipeline | Coding Agent writes here |
| `tests/` | Pipeline | Test Agent writes here |
| `Cargo.toml` | Pipeline (first run) | Created by Planner on first build; human may edit |

## Pipeline

Built by [SpecDFJenkinsPipe](https://github.com/TheChantingMachman/SpecDFJenkinsPipe)
on Jenkins at `http://192.168.1.200:8080/`.

Jenkins job: **Terdis-TDD-DF** (pending creation)
Parameters for first run: `REPO_NAME=Terdis`, `REPO_URL=https://github.com/TheChantingMachman/Terdis.git`,
`BASE_BRANCH=main`, `PIPELINE_MODE=build`, `SCOPE_COMPLEXITY=minimal`

### What the pipeline does

1. Reads `df-config.json` for language/test config
2. Reads `spec/spec.yaml` via `specdb` to understand what needs building
3. Planner writes `work-order.json` scoping a batch of spec entries (includes `spec_ids`)
4. Test Agent writes Rust integration tests to `tests/`
5. Coding Agent implements `src/` to pass the tests (never sees test files)
6. On success: `update-spec-status.sh` marks `spec_ids` as `implemented` in spec.yaml
7. PR created — spec status update + code + tests land atomically on merge

### Mode detection

- **Build mode**: spec unchanged or only new entries added → Planner runs
- **Refactor mode**: existing entries modified/removed → Spec Diff Agent runs first
- Status-only changes (draft→implemented from a previous build) are ignored

## Updating specdb.py

When SpecDB releases a new version (PR merged on main):
```bash
cp /home/anon/dev/SpecDB/src/specdb.py /home/anon/dev/terdis/spec/specdb.py
cd /home/anon/dev/terdis && git add spec/specdb.py && git commit -m "chore: update specdb.py to build-N"
git push
```

## Cargo bootstrap note

`Cargo.toml` does not yet exist. The Planner agent will create it on the first pipeline
build. If bootstrapping manually, a minimal `Cargo.toml` is:

```toml
[package]
name = "terdis"
version = "0.1.0"
edition = "2021"

[lib]
name = "terdis"
path = "src/lib.rs"
```

Integration tests in `tests/` reference the crate as `use terdis::...`.
