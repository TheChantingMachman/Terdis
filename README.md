# Terdis

A Rust reimplementation of Redis, built with the Dark Factory agentic TDD pipeline.

## Pipeline

This repo is built by [SpecDFJenkinsPipe](https://github.com/TheChantingMachman/SpecDFJenkinsPipe).
The pipeline clones this repo, reads `df-config.json`, and orchestrates Claude agents to
implement code from `spec/spec.yaml` using test-first development.

- `df-config.json` — active pipeline configuration for this repo
- `df-config.template.jsonc` — documented schema for all config fields (reference)
- `spec/spec.yaml` — authoritative spec (human-maintained, agent-readable via specdb)
- `spec/specdb.py` — spec management CLI (`python3 spec/specdb.py --help`)

## Structure

```
src/          — Rust source (Cargo crate)
tests/        — integration tests (written by Test Agent, reviewed by human)
spec/         — specification files (human-owned)
df-config.json — pipeline configuration
```

## Pipeline details

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
cp /home/anon/dev/SpecDB/src/specdb.py /home/anon/dev/Terdis/spec/specdb.py
cd /home/anon/dev/Terdis && git add spec/specdb.py && git commit -m "chore: update specdb.py to build-N"
git push
```
