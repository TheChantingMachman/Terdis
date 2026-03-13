#!/usr/bin/env python3
"""SpecDB — Structured Spec Database CLI.

Reads spec/spec.yaml (or --spec-dir/spec.yaml) and provides a query interface
optimized for AI agents in the Dark Factory pipeline.
"""

import argparse
import json
import os
import sys

SPEC_STARTER_TEMPLATE = """\
# spec/spec.yaml — [Project Name] specification
#
# AUTHORSHIP: Maintained by human + AI pair only. Pipeline agents must not
# modify this file. src/ and tests/ are pipeline-owned; spec/ is human-owned.
#
# PIPELINE: This file is the authoritative spec. Pipeline agents access it via
# specdb commands — they do not read this file directly.
# specdb is installed on the build server. Reference: https://github.com/TheChantingMachman/SpecDB
#
# LIFECYCLE: draft → active → implemented
# Run `specdb query --status draft --format brief` to see remaining work.

# ─── REPLACE THIS SECTION WITH YOUR PROJECT'S ENTRIES ───────────────────────
#
# Entry id convention: section_slug.feature_name (snake_case, dot-separated)
# Tag convention: section slug + behavior tags (3-6 per entry, shared across entries)
# Description: be precise — include formulas, exact values, edge cases.
#   The coding agent reads this directly. Vague descriptions produce wrong code.
#
# Example entries below use a grid game domain. Replace with your project's domain.
# ─────────────────────────────────────────────────────────────────────────────

- id: board.dimensions
  section: Board
  description: >
    The board is a 2D grid of cells. Width is 10 columns, height is 20 rows.
    Cells are indexed [row][col], zero-based from the top-left.
    A cell is either empty (value 0) or occupied (value 1–7, representing piece color).
    The board initializes fully empty.
  tags: [board, dimensions, grid]
  status: draft
  constants:
    width: 10
    height: 20

- id: board.line_clear
  section: Board
  description: >
    A line is complete when all cells in a row are occupied (no empty cells).
    On each tick after a piece locks, scan all rows bottom-to-top.
    Remove all complete rows, shift remaining rows down by the number of cleared rows.
    Return the count of rows cleared (0–4). Never modifies rows above the topmost cleared row.
  tags: [board, line-clear, tick]
  status: draft
  depends_on:
    - board.dimensions

- id: scoring.line_clear_points
  section: Scoring
  description: >
    Points awarded for clearing lines in a single move: base_points[lines_cleared] * level.
    base_points table: 1 line = 100, 2 lines = 300, 3 lines = 500, 4 lines = 800.
    Clearing 0 lines awards 0 points. Level is the current level at time of clear (minimum 1).
  tags: [scoring, formula, line-clear, level]
  status: draft
  constants:
    single: 100
    double: 300
    triple: 500
    tetris: 800
  depends_on:
    - board.line_clear
    - game_state.level

- id: game_state.level
  section: Game State
  description: >
    The game starts at level 1. Level increases by 1 for every 10 lines cleared
    (total across the game session, not per-level). Level has no maximum.
    Level affects scoring multiplier and piece fall speed.
  tags: [game-state, level, progression]
  status: draft
  depends_on:
    - board.line_clear

- id: game_state.game_over
  section: Game State
  description: >
    Game over is triggered when a newly spawned piece overlaps any occupied cell
    on the board at spawn time. The game state transitions to GAME_OVER.
    No further ticks, input, or scoring occur after GAME_OVER is set.
    The final score is preserved.
  tags: [game-state, game-over, spawn]
  status: draft
  depends_on:
    - board.dimensions

- id: input.rotation_cw
  section: Input
  description: >
    Clockwise rotation of the active piece. The piece rotates 90 degrees clockwise
    around its center cell. If the rotated position overlaps occupied cells or is
    out of bounds, the rotation is rejected and the piece stays in its current state.
    No wall-kick — rejected rotations are silent no-ops.
  tags: [input, rotation, collision]
  status: draft
  depends_on:
    - board.dimensions
"""

try:
    import yaml
except ImportError:
    print("error: PyYAML is required. Install with: pip install pyyaml", file=sys.stderr)
    sys.exit(1)


def load_spec(spec_dir):
    """Load and parse spec/spec.yaml from spec_dir. Returns list of entry dicts."""
    spec_path = os.path.join(spec_dir, "spec.yaml")
    if not os.path.exists(spec_path):
        print(f"error: spec file not found: {spec_path}", file=sys.stderr)
        sys.exit(1)
    try:
        with open(spec_path) as f:
            entries = yaml.safe_load(f)
    except yaml.YAMLError as e:
        print(f"error: malformed YAML in {spec_path}: {e}", file=sys.stderr)
        sys.exit(1)
    if not isinstance(entries, list):
        print(f"error: {spec_path} must be a YAML list of entries", file=sys.stderr)
        sys.exit(1)
    valid = []
    for entry in entries:
        missing = [f for f in ("id", "section", "description", "tags", "status") if f not in entry]
        if missing:
            print(f"warning: entry missing fields {missing}, skipping: {entry.get('id', '<no id>')}", file=sys.stderr)
            continue
        valid.append(entry)
    return valid


def format_entries(entries, fmt):
    """Format a list of entries as yaml, json, or brief."""
    if fmt == "json":
        # Always emit valid JSON — empty list is [] not silence
        print(json.dumps(entries, indent=2))
    elif fmt == "brief":
        for e in entries:
            desc = e.get("description", "").replace("\n", " ")[:80]
            print(f"{e['id']} | {e['section']} | {e['status']} | {desc}")
    else:  # yaml (default)
        if entries:
            print(yaml.dump(entries, default_flow_style=False, sort_keys=False), end="")


def cmd_query(args):
    entries = load_spec(args.spec_dir)
    if args.id:
        entries = [e for e in entries if e["id"] == args.id]
    if args.section:
        entries = [e for e in entries if e["section"] == args.section]
    if args.tags:
        filter_tags = set(t.strip() for t in args.tags.split(","))
        entries = [e for e in entries if filter_tags & set(e.get("tags", []))]
    if args.status:
        entries = [e for e in entries if e["status"] == args.status]
    format_entries(entries, args.format)


def cmd_sections(args):
    entries = load_spec(args.spec_dir)
    seen = []
    for e in entries:
        if e["section"] not in seen:
            seen.append(e["section"])
    for s in seen:
        print(s)


def cmd_tags(args):
    entries = load_spec(args.spec_dir)
    all_tags = set()
    for e in entries:
        all_tags.update(e.get("tags", []))
    for t in sorted(all_tags):
        print(t)


def load_snapshot(path):
    """Load a spec snapshot from an arbitrary YAML file path. Returns list of entry dicts."""
    if not os.path.exists(path):
        print(f"error: snapshot file not found: {path}", file=sys.stderr)
        sys.exit(1)
    try:
        with open(path) as f:
            entries = yaml.safe_load(f)
    except yaml.YAMLError as e:
        print(f"error: malformed YAML in snapshot {path}: {e}", file=sys.stderr)
        sys.exit(1)
    if not isinstance(entries, list):
        print(f"error: snapshot {path} must be a YAML list of entries", file=sys.stderr)
        sys.exit(1)
    return {e["id"]: e for e in entries if isinstance(e, dict) and "id" in e}


def format_diff(changes, fmt):
    """Format diff results. Each change has change_type plus entry fields."""
    if fmt == "json":
        print(json.dumps(changes, indent=2))
        return
    if not changes:
        return
    elif fmt == "brief":
        symbols = {"added": "+", "removed": "-", "modified": "~"}
        for c in changes:
            sym = symbols.get(c["change_type"], "?")
            entry = c.get("new", c) if c["change_type"] == "modified" else c
            section = entry.get("section", "")
            print(f"[{sym}] {c['id']} | {section}")
    else:  # yaml
        print(yaml.dump(changes, default_flow_style=False, sort_keys=False), end="")


def cmd_diff(args):
    current_entries = load_spec(args.spec_dir)
    snapshot = load_snapshot(args.snapshot)
    current = {e["id"]: e for e in current_entries}

    changes = []

    for eid, entry in current.items():
        if eid not in snapshot:
            changes.append({"change_type": "added", **entry})
        elif entry != snapshot[eid]:
            if getattr(args, "ignore_status", False):
                old_no_status = {k: v for k, v in snapshot[eid].items() if k != "status"}
                new_no_status = {k: v for k, v in entry.items() if k != "status"}
                if old_no_status == new_no_status:
                    continue  # status-only change — pipeline transition, not human edit
            changes.append({"change_type": "modified", "id": eid, "old": snapshot[eid], "new": entry})

    for eid, entry in snapshot.items():
        if eid not in current:
            changes.append({"change_type": "removed", **entry})

    format_diff(changes, args.format)


def cmd_update(args):
    spec_path = os.path.join(args.spec_dir, "spec.yaml")
    if not os.path.exists(spec_path):
        print(f"error: spec file not found: {spec_path}", file=sys.stderr)
        sys.exit(1)
    try:
        with open(spec_path) as f:
            raw_lines = f.readlines()
    except OSError as e:
        print(f"error: cannot read {spec_path}: {e}", file=sys.stderr)
        sys.exit(1)

    # Separate leading comment lines from yaml body
    leading_comments = []
    yaml_body_lines = []
    in_comments = True
    for line in raw_lines:
        if in_comments and line.startswith("#"):
            leading_comments.append(line)
        else:
            in_comments = False
            yaml_body_lines.append(line)

    yaml_body = "".join(yaml_body_lines)
    try:
        entries = yaml.safe_load(yaml_body)
    except yaml.YAMLError as e:
        print(f"error: malformed YAML in {spec_path}: {e}", file=sys.stderr)
        sys.exit(1)
    if not isinstance(entries, list):
        print(f"error: {spec_path} must be a YAML list of entries", file=sys.stderr)
        sys.exit(1)

    # Check for immutable id field before any file I/O
    if args.set:
        for kv in args.set:
            key, _, _ = kv.partition("=")
            if key == "id":
                print("error: 'id' is immutable and cannot be changed with --set", file=sys.stderr)
                sys.exit(1)

    # Find entry by id
    target = None
    for entry in entries:
        if isinstance(entry, dict) and entry.get("id") == args.id:
            target = entry
            break
    if target is None:
        print(f"error: entry not found: {args.id}", file=sys.stderr)
        sys.exit(1)

    # Apply --status
    if args.status:
        target["status"] = args.status

    # Apply --set key=value pairs
    if args.set:
        for kv in args.set:
            key, _, value = kv.partition("=")
            target[key] = value

    with open(spec_path, "w") as f:
        f.writelines(leading_comments)
        yaml.dump(entries, f, default_flow_style=False, sort_keys=False)


def cmd_stale(args):
    current_entries = load_spec(args.spec_dir)
    snapshot = load_snapshot(args.snapshot)
    current = {e["id"]: e for e in current_entries}

    # Collect changed entries: modified + removed (not added)
    changed_entries = {}  # id -> entry (new version for modified, old for removed)
    for eid, entry in current.items():
        if eid in snapshot and entry != snapshot[eid]:
            changed_entries[eid] = entry  # use new version
    for eid, entry in snapshot.items():
        if eid not in current:
            changed_entries[eid] = entry  # removed

    if not changed_entries:
        if args.format == "json":
            print("[]")
        return

    # Collect all tags from changed entries
    changed_tags = set()
    for entry in changed_entries.values():
        changed_tags.update(entry.get("tags", []))

    test_dir = args.test_dir
    test_ext = args.test_ext

    stale_files = []  # list of (fpath, [entry_ids that triggered it])

    if os.path.isdir(test_dir):
        for root, dirs, files in os.walk(test_dir):
            dirs.sort()
            for fname in sorted(files):
                if not fname.endswith(test_ext):
                    continue
                fpath = os.path.join(root, fname)
                file_tags = set()
                try:
                    with open(fpath, errors="replace") as f:
                        for i, line in enumerate(f):
                            if i >= 10:
                                break
                            line = line.strip()
                            for prefix in ("# @spec-tags:", "// @spec-tags:"):
                                if line.startswith(prefix):
                                    tag_part = line[len(prefix):]
                                    file_tags.update(t.strip() for t in tag_part.split(",") if t.strip())
                except OSError:
                    continue
                if file_tags & changed_tags:
                    # Determine which changed entry IDs triggered this
                    triggering_ids = []
                    for eid, entry in changed_entries.items():
                        if file_tags & set(entry.get("tags", [])):
                            triggering_ids.append(eid)
                    stale_files.append((fpath, triggering_ids))

    if args.format == "json":
        result = [{"file": f, "stale_because": ids} for f, ids in stale_files]
        print(json.dumps(result, indent=2))
    else:  # brief
        for fpath, _ in stale_files:
            print(fpath)


def cmd_tests(args):
    if not args.tags and not args.section:
        print("error: --tags or --section is required", file=sys.stderr)
        sys.exit(1)

    query_tags = set()
    if args.tags:
        query_tags.update(t.strip() for t in args.tags.split(","))
    if args.section:
        # Load spec to resolve section -> tags mapping
        entries = load_spec(args.spec_dir)
        for e in entries:
            if e["section"] == args.section:
                query_tags.update(e.get("tags", []))

    test_dir = args.test_dir
    test_ext = args.test_ext

    if not os.path.isdir(test_dir):
        return

    for root, dirs, files in os.walk(test_dir):
        dirs.sort()
        for fname in sorted(files):
            if not fname.endswith(test_ext):
                continue
            fpath = os.path.join(root, fname)
            file_tags = set()
            try:
                with open(fpath, errors="replace") as f:
                    for i, line in enumerate(f):
                        if i >= 10:
                            break
                        line = line.strip()
                        for prefix in ("# @spec-tags:", "// @spec-tags:"):
                            if line.startswith(prefix):
                                tag_part = line[len(prefix):]
                                file_tags.update(t.strip() for t in tag_part.split(",") if t.strip())
            except OSError:
                continue
            if file_tags & query_tags:
                print(fpath)


def cmd_init(args):
    spec_dir = "spec"
    spec_path = os.path.join(spec_dir, "spec.yaml")
    config_path = "df-config.json"

    conflicts = []
    if os.path.exists(spec_path):
        conflicts.append(spec_path)
    if os.path.exists(config_path):
        conflicts.append(config_path)
    if conflicts:
        for p in conflicts:
            print(f"error: file already exists: {p}", file=sys.stderr)
        sys.exit(1)

    os.makedirs(spec_dir, exist_ok=True)
    with open(spec_path, "w") as f:
        f.write(SPEC_STARTER_TEMPLATE)

    config = {
        "project_name": args.project_name,
        "language": args.language,
        "test_framework": args.test_framework,
        "src_dir": args.src_dir,
        "test_dir": args.test_dir,
        "build_test_cmd": args.build_test_cmd,
        "spec_dir": "",
        "spec_file": "",
        "src_ext": "",
        "test_ext": "",
        "report_format": "",
        "src_include": [],
        "src_exclude": [],
        "test_pattern": "",
        "planner_hint": "",
        "test_hint": "",
        "coding_hint": "",
    }
    with open(config_path, "w") as f:
        json.dump(config, f, indent=2)
        f.write("\n")

    print(spec_path)
    print(config_path)


def main():
    parser = argparse.ArgumentParser(
        prog="specdb",
        description="SpecDB — query and manage structured project specifications.",
    )
    parser.add_argument(
        "--spec-dir",
        default="spec",
        help="Directory containing spec.yaml (default: spec/)",
    )

    sub = parser.add_subparsers(dest="command", required=False)

    # query
    p_query = sub.add_parser("query", help="Return spec entries matching filters.")
    p_query.add_argument("--id", help="Return single entry by ID.")
    p_query.add_argument("--section", help="Filter by section (exact match).")
    p_query.add_argument("--tags", help="Filter by tags (comma-separated, any match).")
    p_query.add_argument("--status", help="Filter by status (draft|active|implemented).")
    p_query.add_argument("--format", choices=["yaml", "json", "brief"], default="yaml", help="Output format.")
    p_query.set_defaults(func=cmd_query)

    # sections
    p_sections = sub.add_parser("sections", help="List all unique sections.")
    p_sections.set_defaults(func=cmd_sections)

    # tags
    p_tags = sub.add_parser("tags", help="List all unique tags (sorted).")
    p_tags.set_defaults(func=cmd_tags)

    # diff
    p_diff = sub.add_parser("diff", help="Compare spec against a snapshot.")
    p_diff.add_argument("--snapshot", required=True, help="Path to previous spec snapshot.")
    p_diff.add_argument("--format", choices=["yaml", "json", "brief"], default="yaml")
    p_diff.add_argument("--ignore-status", action="store_true",
                        help="Exclude entries where the only change is the status field (pipeline-caused transitions).")
    p_diff.set_defaults(func=cmd_diff)

    # tests
    p_tests = sub.add_parser("tests", help="Find test files related to spec entries.")
    p_tests.add_argument("--tags", help="Find tests covering these tags (comma-separated).")
    p_tests.add_argument("--section", help="Find tests covering this section.")
    p_tests.add_argument("--test-dir", default="tests", help="Test directory to scan.")
    p_tests.add_argument("--test-ext", default=".py", help="Test file extension (default: .py).")
    p_tests.set_defaults(func=cmd_tests)

    # update
    p_update = sub.add_parser("update", help="Modify a single spec entry in-place.")
    p_update.add_argument("--id", required=True, help="Entry ID to update.")
    p_update.add_argument("--status", help="Set status field (draft|active|implemented).")
    p_update.add_argument("--set", action="append", metavar="key=value", help="Set an arbitrary field (may be used multiple times).")
    p_update.set_defaults(func=cmd_update)

    # stale
    p_stale = sub.add_parser("stale", help="Find stale test files from spec changes.")
    p_stale.add_argument("--snapshot", required=True, help="Path to previous spec snapshot.")
    p_stale.add_argument("--test-dir", default="tests", help="Test directory to scan.")
    p_stale.add_argument("--test-ext", default=".py", help="Test file extension (default: .py).")
    p_stale.add_argument("--format", choices=["brief", "json"], default="brief")
    p_stale.set_defaults(func=cmd_stale)

    # init
    p_init = sub.add_parser("init", help="Scaffold a new project (spec/spec.yaml + df-config.json).")
    p_init.add_argument("--project-name", default="", help="Project name.")
    p_init.add_argument("--language", default="", help="Primary language.")
    p_init.add_argument("--test-framework", default="", help="Test framework.")
    p_init.add_argument("--src-dir", default="src/", help="Source directory (default: src/).")
    p_init.add_argument("--test-dir", default="tests/", help="Test directory (default: tests/).")
    p_init.add_argument("--build-test-cmd", default="", help="Command to build and run tests.")
    p_init.set_defaults(func=cmd_init)

    args = parser.parse_args()

    if args.command is None:
        print("specdb — query and manage structured project specifications.")
        print()
        print("First-time setup:")
        print("  specdb init [--project-name NAME] [--language LANG] [--test-framework FW]")
        print("              [--src-dir DIR] [--test-dir DIR] [--build-test-cmd CMD]")
        print()
        print("For full usage: specdb --help")
        sys.exit(0)

    args.func(args)


if __name__ == "__main__":
    main()
