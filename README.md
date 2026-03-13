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
