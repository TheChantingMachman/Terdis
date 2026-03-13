# Claude Code — Terdis Project Instructions

## Spec Management

The authoritative spec lives at `spec/spec.yaml`. **Do not read this file directly** — as
the spec grows it will overwhelm your context window.

Instead, use the specdb CLI to query and modify the spec:

```bash
# Orient — list sections and tags
python3 spec/specdb.py sections
python3 spec/specdb.py tags

# Browse — one-line summaries (minimal tokens)
python3 spec/specdb.py query --format brief
python3 spec/specdb.py query --status draft --format brief

# Drill in — read specific entries
python3 spec/specdb.py query --id commands.set
python3 spec/specdb.py query --section Commands --format yaml
python3 spec/specdb.py query --tags "core,write" --format yaml

# Modify — update fields on existing entries
python3 spec/specdb.py update --id commands.set --status active
python3 spec/specdb.py update --id commands.set --set "description=new text"

# Add — create new entries (if available)
python3 spec/specdb.py add --id commands.ping --section Commands \
  --description "PING returns PONG." --tags "commands,ping" --status draft
```

Only read `spec/spec.yaml` directly if specdb is broken or you need to fix YAML syntax errors.

## Project Context

- Terdis is a Rust reimplementation of Redis
- `df-config.json` configures the Dark Factory agentic TDD pipeline
- `spec/` is human-owned; `src/` and `tests/` are pipeline-owned
- The pipeline generates tests first, then implementation code to pass them
