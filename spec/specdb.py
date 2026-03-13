#!/usr/bin/env python3
"""SpecDB — Structured Spec Database CLI.

Reads spec/spec.yaml (or --spec-dir/spec.yaml) and provides a query interface
optimized for AI agents in the Dark Factory pipeline.
"""

import argparse
import json
import os
import sys

VALID_STATUSES = frozenset({"draft", "active", "implemented", "deprecated"})

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
# LIFECYCLE: draft → active → implemented | deprecated
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


# ---------------------------------------------------------------------------
# Raw file I/O helpers for round-trip preservation
# ---------------------------------------------------------------------------

def _split_leading_comments(raw_lines):
    """Split raw lines into (leading_comments, body_lines).

    leading_comments: lines at the top of the file that start with '#'.
    body_lines: all remaining lines (includes blank separator and entries).
    """
    leading_comments = []
    body_lines = []
    in_comments = True
    for line in raw_lines:
        if in_comments and line.startswith("#"):
            leading_comments.append(line)
        else:
            in_comments = False
            body_lines.append(line)
    return leading_comments, body_lines


def _find_entry_starts(body_lines):
    """Return indices in body_lines where each top-level YAML list entry starts.

    A top-level entry starts with '- ' at column 0.
    """
    starts = []
    for i, line in enumerate(body_lines):
        if line.startswith("- ") or line.rstrip("\n") == "-":
            starts.append(i)
    return starts


def _entry_range(body_lines, entry_starts, idx):
    """Return (start, end) slice indices for entry at position idx."""
    start = entry_starts[idx]
    end = entry_starts[idx + 1] if idx + 1 < len(entry_starts) else len(body_lines)
    return start, end


def _serialize_entry(entry):
    """Serialize a single entry dict to a canonical YAML string (list-item format).

    Uses a fixed field order and canonical formatting — not yaml.dump().
    Returns a string ending with exactly one newline and no trailing blank line.
    Callers are responsible for inter-entry blank line separators.
    """
    CANONICAL_FIELDS = ["id", "section", "description", "tags", "status", "depends_on", "constants"]

    def needs_quoting(s):
        s = str(s)
        if s != s.strip():
            return True
        if ":" in s or "#" in s:
            return True
        return False

    def plain_scalar(v):
        s = str(v)
        if needs_quoting(s):
            escaped = s.replace("\\", "\\\\").replace('"', '\\"')
            return f'"{escaped}"'
        return s

    def wrap_text(text, width=74):
        """Word-wrap text to lines of at most `width` chars. Returns list of lines."""
        words = text.split()
        lines = []
        current = []
        current_len = 0
        for word in words:
            if current and current_len + 1 + len(word) > width:
                lines.append(" ".join(current))
                current = [word]
                current_len = len(word)
            else:
                if current:
                    current_len += 1 + len(word)
                else:
                    current_len = len(word)
                current.append(word)
        if current:
            lines.append(" ".join(current))
        return lines

    def serialize_description(text):
        """Return list of output lines for description folded scalar content."""
        text = text.rstrip("\n")
        paragraphs = text.split("\n\n")
        all_lines = []
        for i, para in enumerate(paragraphs):
            if i > 0:
                all_lines.append("")  # blank line separator between paragraphs
            para_text = para.replace("\n", " ").strip()
            if para_text:
                all_lines.extend(wrap_text(para_text, width=74))
        return all_lines

    # Determine field order: canonical fields first, then extras in dict order
    ordered_keys = [k for k in CANONICAL_FIELDS if k in entry]
    ordered_keys += [k for k in entry if k not in CANONICAL_FIELDS]

    output_lines = []
    first_field = True

    for key in ordered_keys:
        value = entry[key]
        # Skip None and empty collections
        if value is None:
            continue
        if isinstance(value, (list, dict)) and len(value) == 0:
            continue

        indent = "- " if first_field else "  "
        first_field = False

        if key == "description":
            desc_lines = serialize_description(str(value))
            output_lines.append(f"{indent}description: >")
            for dline in desc_lines:
                if dline == "":
                    output_lines.append("")
                else:
                    output_lines.append(f"    {dline}")
        elif key == "tags":
            tags_str = "[" + ", ".join(str(t) for t in value) + "]"
            output_lines.append(f"{indent}tags: {tags_str}")
        elif key == "depends_on":
            output_lines.append(f"{indent}depends_on:")
            for dep in value:
                output_lines.append(f"  - {plain_scalar(dep)}")
        elif key == "constants":
            output_lines.append(f"{indent}constants:")
            for k, v in value.items():
                output_lines.append(f"    {k}: {plain_scalar(v)}")
        else:
            output_lines.append(f"{indent}{key}: {plain_scalar(value)}")

    result = "\n".join(output_lines)
    if not result.endswith("\n"):
        result += "\n"
    return result


def _parse_spec_file(spec_path):
    """Read spec.yaml and return (leading_comments, body_lines, entries).

    entries is the parsed list of dicts (may include invalid ones).
    Exits on I/O or YAML errors.
    """
    try:
        with open(spec_path) as f:
            raw_lines = f.readlines()
    except OSError as e:
        print(f"error: cannot read {spec_path}: {e}", file=sys.stderr)
        sys.exit(1)

    leading_comments, body_lines = _split_leading_comments(raw_lines)

    yaml_body = "".join(body_lines)
    try:
        entries = yaml.safe_load(yaml_body)
    except yaml.YAMLError as e:
        print(f"error: malformed YAML in {spec_path}: {e}", file=sys.stderr)
        sys.exit(1)
    if not isinstance(entries, list):
        entries = []

    return leading_comments, body_lines, entries


def _write_spec_file(spec_path, leading_comments, body_lines):
    """Write leading_comments + body_lines to spec_path."""
    with open(spec_path, "w") as f:
        f.writelines(leading_comments)
        f.writelines(body_lines)


# ---------------------------------------------------------------------------
# Spec loading (for read-only commands)
# ---------------------------------------------------------------------------

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


# ---------------------------------------------------------------------------
# Commands
# ---------------------------------------------------------------------------

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
    if getattr(args, "with_counts", False):
        tag_counts = {}
        for e in entries:
            for t in e.get("tags", []):
                tag_counts[t] = tag_counts.get(t, 0) + 1
        # Sort by count descending, then tag alphabetically
        sorted_tags = sorted(tag_counts.items(), key=lambda x: (-x[1], x[0]))
        for tag, count in sorted_tags:
            print(f"{count}\t{tag}")
    else:
        all_tags = set()
        for e in entries:
            all_tags.update(e.get("tags", []))
        for t in sorted(all_tags):
            print(t)


def load_snapshot(path):
    """Load a spec snapshot from an arbitrary YAML file path. Returns dict of id -> entry."""
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

    # Full current map (including deprecated) — needed for removed-entry check
    current_all = {e["id"]: e for e in current_entries}
    # Active (non-deprecated) current entries — only these appear in diff output
    current_active = {eid: e for eid, e in current_all.items()
                      if e.get("status") != "deprecated"}

    changes = []

    for eid, entry in current_active.items():
        if eid not in snapshot:
            changes.append({"change_type": "added", **entry})
        elif entry != snapshot[eid]:
            if getattr(args, "ignore_status", False):
                old_no_status = {k: v for k, v in snapshot[eid].items() if k != "status"}
                new_no_status = {k: v for k, v in entry.items() if k != "status"}
                if old_no_status == new_no_status:
                    continue  # status-only change — pipeline transition, not human edit
            changes.append({"change_type": "modified", "id": eid, "old": snapshot[eid], "new": entry})

    for eid, snap_entry in snapshot.items():
        if eid not in current_all:
            # Truly removed from spec — skip if it was deprecated in snapshot
            if snap_entry.get("status") != "deprecated":
                changes.append({"change_type": "removed", **snap_entry})
        # If eid is in current but deprecated: already excluded from current_active,
        # so it won't appear as modified. And it's not "removed" either.

    format_diff(changes, args.format)


def cmd_update(args):
    spec_path = os.path.join(args.spec_dir, "spec.yaml")
    if not os.path.exists(spec_path):
        print(f"error: spec file not found: {spec_path}", file=sys.stderr)
        sys.exit(1)

    # Validate status before any I/O
    if args.status and args.status not in VALID_STATUSES:
        print(
            f"error: invalid status '{args.status}': must be one of {sorted(VALID_STATUSES)}",
            file=sys.stderr,
        )
        sys.exit(1)

    # Check for immutable id field before any file I/O
    if args.set:
        for kv in args.set:
            key, _, _ = kv.partition("=")
            if key == "id":
                print("error: 'id' is immutable and cannot be changed with --set", file=sys.stderr)
                sys.exit(1)

    leading_comments, body_lines, entries = _parse_spec_file(spec_path)

    # Find target entry by id
    target_idx = None
    target_entry = None
    for i, entry in enumerate(entries):
        if isinstance(entry, dict) and entry.get("id") == args.id:
            target_idx = i
            target_entry = entry
            break
    if target_entry is None:
        print(f"error: entry not found: {args.id}", file=sys.stderr)
        sys.exit(1)

    # Apply changes to the entry dict
    if args.status:
        target_entry["status"] = args.status
    if args.set:
        for kv in args.set:
            key, _, value = kv.partition("=")
            target_entry[key] = value

    # Round-trip: find raw line boundaries for this entry
    entry_starts = _find_entry_starts(body_lines)
    start, end = _entry_range(body_lines, entry_starts, target_idx)

    # Determine which fields actually changed (only the keys touched by this call)
    changed_keys = set()
    if args.status:
        changed_keys.add("status")
    if args.set:
        for kv in args.set:
            key, _, _ = kv.partition("=")
            changed_keys.add(key)

    # Simple scalar fields that can be safely spliced in-place without re-serializing
    # the whole entry.  Structural fields (depends_on, constants, description, tags)
    # require full re-serialization because their YAML representation spans multiple lines.
    INLINE_SPLICEABLE = {"status", "section"}

    if changed_keys and changed_keys.issubset(INLINE_SPLICEABLE):
        # Build a mapping of field → new value for the changed keys
        new_values = {}
        if args.status:
            new_values["status"] = args.status
        if args.set:
            for kv in args.set:
                key, _, value = kv.partition("=")
                if key in INLINE_SPLICEABLE:
                    new_values[key] = value

        # Splice only the specific lines that contain the changed fields
        new_body = list(body_lines)
        for line_idx in range(start, end):
            stripped = new_body[line_idx].lstrip()
            for field, new_val in new_values.items():
                if stripped.startswith(f"{field}:"):
                    indent = len(new_body[line_idx]) - len(stripped)
                    new_body[line_idx] = " " * indent + f"{field}: {new_val}\n"
                    break
        _write_spec_file(spec_path, leading_comments, new_body)
        return

    # Full re-serialization for structural changes (description, tags, depends_on, constants)
    # Preserve trailing blank lines from original range (inter-entry separators)
    trailing_blanks = []
    for line in reversed(body_lines[start:end]):
        if line.strip() == "":
            trailing_blanks.insert(0, line)
        else:
            break

    new_yaml_lines = _serialize_entry(target_entry).splitlines(keepends=True)

    # Splice: untouched prefix + new entry + original trailing blanks + untouched suffix
    new_body = body_lines[:start] + new_yaml_lines + trailing_blanks + body_lines[end:]
    _write_spec_file(spec_path, leading_comments, new_body)


def cmd_add(args):
    spec_path = os.path.join(args.spec_dir, "spec.yaml")
    if not os.path.exists(spec_path):
        print(f"error: spec file not found: {spec_path}", file=sys.stderr)
        sys.exit(1)

    # Validate status before any I/O
    if args.status not in VALID_STATUSES:
        print(
            f"error: invalid status '{args.status}': must be one of {sorted(VALID_STATUSES)}",
            file=sys.stderr,
        )
        sys.exit(1)

    leading_comments, body_lines, entries = _parse_spec_file(spec_path)

    # Check for duplicate ID
    for entry in entries:
        if isinstance(entry, dict) and entry.get("id") == args.id:
            print(f"error: entry with id '{args.id}' already exists", file=sys.stderr)
            sys.exit(1)

    # Build new entry
    new_entry = {
        "id": args.id,
        "section": args.section,
        "description": args.description,
        "tags": [t.strip() for t in args.tags.split(",")],
        "status": args.status,
    }
    if args.depends_on:
        new_entry["depends_on"] = [t.strip() for t in args.depends_on.split(",")]
    if args.constants:
        constants = {}
        for kv in args.constants:
            key, _, value = kv.partition("=")
            constants[key] = value
        new_entry["constants"] = constants

    # Serialize the new entry
    new_yaml_lines = _serialize_entry(new_entry).splitlines(keepends=True)

    # Strip trailing blank lines from body, then add exactly one blank separator
    stripped_body = list(body_lines)
    while stripped_body and stripped_body[-1].strip() == "":
        stripped_body.pop()

    new_body = stripped_body + ["\n"] + new_yaml_lines
    _write_spec_file(spec_path, leading_comments, new_body)


def cmd_remove(args):
    spec_path = os.path.join(args.spec_dir, "spec.yaml")
    if not os.path.exists(spec_path):
        print(f"error: spec file not found: {spec_path}", file=sys.stderr)
        sys.exit(1)

    leading_comments, body_lines, entries = _parse_spec_file(spec_path)

    # Find target entry index
    target_idx = None
    for i, entry in enumerate(entries):
        if isinstance(entry, dict) and entry.get("id") == args.id:
            target_idx = i
            break
    if target_idx is None:
        print(f"error: entry not found: {args.id}", file=sys.stderr)
        sys.exit(1)

    # Check which other entries depend on this one
    dependents = []
    for entry in entries:
        if isinstance(entry, dict) and entry.get("id") != args.id:
            if args.id in entry.get("depends_on", []):
                dependents.append(entry["id"])

    if dependents and not args.force:
        print(
            f"error: cannot remove '{args.id}': referenced by {dependents}",
            file=sys.stderr,
        )
        sys.exit(1)

    if dependents and args.force:
        print(
            f"warning: removing '{args.id}' which is referenced by: {dependents}",
            file=sys.stderr,
        )

    # Round-trip: find raw line boundaries for this entry
    entry_starts = _find_entry_starts(body_lines)
    start, end = _entry_range(body_lines, entry_starts, target_idx)

    # Remove entry range (includes the trailing blank line following the entry)
    new_body = body_lines[:start] + body_lines[end:]
    _write_spec_file(spec_path, leading_comments, new_body)


def cmd_validate(args):
    spec_path = os.path.join(args.spec_dir, "spec.yaml")
    if not os.path.exists(spec_path):
        print(f"error: spec file not found: {spec_path}", file=sys.stderr)
        sys.exit(1)

    leading_comments, body_lines, entries = _parse_spec_file(spec_path)

    if not isinstance(entries, list):
        print("error: spec.yaml must be a YAML list of entries", file=sys.stderr)
        sys.exit(1)

    issues = []
    warnings = []
    seen_ids = {}  # id -> index
    all_ids = set()

    # First pass: collect all IDs (for depends_on reference checking)
    for entry in entries:
        if isinstance(entry, dict) and "id" in entry:
            all_ids.add(entry["id"])

    required_fields = ("id", "section", "description", "tags", "status")

    for i, entry in enumerate(entries):
        if not isinstance(entry, dict):
            issues.append(f"<entry {i}>: not a dict")
            continue

        eid = entry.get("id", f"<entry {i}>")

        # Required fields
        for field in required_fields:
            if field not in entry:
                issues.append(f"{eid}: missing required field '{field}'")

        # Valid status
        status = entry.get("status")
        if status is not None and status not in VALID_STATUSES:
            issues.append(f"{eid}: invalid status '{status}'")

        # Duplicate IDs
        if "id" in entry:
            if entry["id"] in seen_ids:
                issues.append(f"{eid}: duplicate id (also at entry {seen_ids[entry['id']]})")
            else:
                seen_ids[entry["id"]] = i

        # depends_on references must exist
        for dep in entry.get("depends_on", []):
            if dep not in all_ids:
                issues.append(f"{eid}: depends_on references unknown id '{dep}'")

        # tags must be a non-empty list
        tags = entry.get("tags")
        if tags is not None:
            if not isinstance(tags, list):
                issues.append(f"{eid}: tags must be a list")
            elif len(tags) == 0:
                issues.append(f"{eid}: tags list must have at least 1 element")

    # Round-trip stability check: re-serialize each entry and compare to raw file content.
    # Mismatches indicate quoting gaps or formatting drift that would cause cosmetic churn
    # on the next structural update.  Emitted as warnings (not errors) since the data is valid.
    entry_starts = _find_entry_starts(body_lines)
    if len(entry_starts) == len(entries):
        for i, entry in enumerate(entries):
            if not isinstance(entry, dict):
                continue
            start, end = _entry_range(body_lines, entry_starts, i)
            # Strip trailing blank lines from the raw range (inter-entry separators are not
            # part of the entry's serialized form)
            raw_content_end = end
            while raw_content_end > start and body_lines[raw_content_end - 1].strip() == "":
                raw_content_end -= 1
            original = "".join(body_lines[start:raw_content_end])
            reserialized = _serialize_entry(entry)
            if original != reserialized:
                eid = entry.get("id", f"<entry {i}>")
                warnings.append(f"{eid}: round-trip mismatch — file format differs from canonical serialization (run 'specdb update --id {eid}' with a structural change to normalize)")

    for issue in issues:
        print(issue)
    for warning in warnings:
        print(f"warning: {warning}", file=sys.stderr)

    if issues:
        sys.exit(1)


def cmd_audit(args):
    """Find test files missing @spec-tags: header in the first 10 lines."""
    test_dir = args.test_dir
    test_ext = args.test_ext

    missing = []

    if os.path.isdir(test_dir):
        for root, dirs, files in os.walk(test_dir):
            dirs.sort()
            for fname in sorted(files):
                if not fname.endswith(test_ext):
                    continue
                fpath = os.path.join(root, fname)
                has_spec_tags = False
                try:
                    with open(fpath, errors="replace") as f:
                        for i, line in enumerate(f):
                            if i >= 10:
                                break
                            stripped = line.strip()
                            if stripped.startswith("# @spec-tags:") or stripped.startswith("// @spec-tags:"):
                                has_spec_tags = True
                                break
                except OSError:
                    continue
                if not has_spec_tags:
                    missing.append(fpath)

    if args.format == "json":
        print(json.dumps(missing))
    else:  # brief
        for fpath in missing:
            print(fpath)


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
    p_query.add_argument("--status", help="Filter by status (draft|active|implemented|deprecated).")
    p_query.add_argument("--format", choices=["yaml", "json", "brief"], default="yaml", help="Output format.")
    p_query.set_defaults(func=cmd_query)

    # sections
    p_sections = sub.add_parser("sections", help="List all unique sections.")
    p_sections.set_defaults(func=cmd_sections)

    # tags
    p_tags = sub.add_parser("tags", help="List all unique tags (sorted).")
    p_tags.add_argument(
        "--with-counts",
        action="store_true",
        default=False,
        help="Output tab-separated count+tag, sorted by count descending then tag alphabetically.",
    )
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
    p_update.add_argument("--status", help="Set status field (draft|active|implemented|deprecated).")
    p_update.add_argument("--set", action="append", metavar="key=value",
                          help="Set an arbitrary field (may be used multiple times).")
    p_update.set_defaults(func=cmd_update)

    # add
    p_add = sub.add_parser("add", help="Append a new entry to spec.yaml.")
    p_add.add_argument("--id", required=True, help="Entry ID (must be unique).")
    p_add.add_argument("--section", required=True, help="Section name.")
    p_add.add_argument("--description", required=True, help="Entry description.")
    p_add.add_argument("--tags", required=True, help="Comma-separated tag list.")
    p_add.add_argument("--status", default="draft",
                       help="Status (draft|active|implemented|deprecated). Default: draft.")
    p_add.add_argument("--depends-on", help="Comma-separated list of entry IDs.")
    p_add.add_argument("--constants", action="append", metavar="key=value",
                       help="Constant key=value pair (may be repeated).")
    p_add.set_defaults(func=cmd_add)

    # remove
    p_remove = sub.add_parser("remove", help="Remove a spec entry by ID.")
    p_remove.add_argument("--id", required=True, help="Entry ID to remove.")
    p_remove.add_argument("--force", action="store_true",
                          help="Remove even if other entries depend on this ID.")
    p_remove.set_defaults(func=cmd_remove)

    # validate
    p_validate = sub.add_parser("validate", help="Validate spec.yaml for structural correctness.")
    p_validate.set_defaults(func=cmd_validate)

    # audit
    p_audit = sub.add_parser("audit", help="Find test files missing @spec-tags: header.")
    p_audit.add_argument("--test-dir", default="tests", help="Test directory to scan (default: tests/).")
    p_audit.add_argument("--test-ext", default=".py", help="Test file extension (default: .py).")
    p_audit.add_argument("--format", choices=["brief", "json"], default="brief")
    p_audit.set_defaults(func=cmd_audit)

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
