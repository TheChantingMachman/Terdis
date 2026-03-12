# Custodian — Dev Log

---

## Session 2026-03-09 — Scope Check Agent + PR review policy

### Scope Check Agent (implemented, on dev)

Lightweight pre-test agent inserted between Planner/Spec Diff and the Test Agent. Reads the
work order + relevant spec sections, produces `${ARTIFACT_DIR}/assumptions.json` — a short
list of resolved ambiguities injected inline into both the Test Agent and Coding Agent prompts.

**Why:** The divergent-inference failure mode (build-36) showed that two agents reading the
same spec can draw different conclusions about unstated-but-implied behavior. Alignment at the
source (before tests are written) is cheaper and more reliable than post-hoc contradiction
detection. This is Candidate C from the TODO list, implemented as a lighter variant.

**Design decisions:**
- Non-fatal: timeout or error falls back to `{"assumptions": [], "spec_gaps": []}` — build continues
- Skipped for minimal build mode (low-ambiguity, fast builds stay fast)
- Assumptions are inline-injected into prompts, not file-read by agents — no extra context tax
- Spec gaps (required behaviors absent from spec) surface in the PR body for human review
- `assumptions.json` lives in ARTIFACT_DIR — cleaned by 15-build rotation, no accumulation
- The PR body is the permanent human-readable record; closed PRs are searchable on GitHub

**Files changed:** `Pipeline/agents/scope-check.sh` (new), `Pipeline/Jenkinsfile`,
`Pipeline/agents/test.sh`, `Pipeline/agents/coding.sh`, `Pipeline/scripts/create-pr.sh`

### Build-44 — large spec refactor succeeded on attempt 1

Build-44 implemented the full Phase 2/3 spec expansion in one run: solar panel degradation,
progressive human power cost, Phase 2 colony cap, tech scrap vault, robot capacity expansion,
Phase 2→3 gate, Phase 3 stubs. 607/607 tests, attempt 1. The scope check agent was not yet
active for this build (merged to dev after the run completed).

### PR review policy: skip it

Across all builds to date, manual PR code review (Claude Code inspecting the diff before merge)
has never found a problem. The conclusion: the test gate is the correct and sufficient quality
gate for pipeline-owned code (`src/custodian.hpp`, `src/custodian.cpp`, `qa/`).

**Why it works without review:**
- Tests are written from spec independently of the implementation — if they agree, it's real
- 607 tests now; coverage is broad enough that a spec deviation shows up as a test failure
- The test+code agents never see each other's output before tests run — no colluding

**Going forward:**
- Merge pipeline PRs after confirming the test count looks reasonable (no review needed)
- **Exception: renderer changes** (`src/renderer/main.cpp`) are human-owned and not Catch2-tested.
  Always smoke-test the binary before merging a PR that touches the renderer.

---

## Session 2026-03-08 (continued) — Pipeline capability progress & TODO

### Progress report: Dark Factory Pipeline — main → dev (builds 23–42)

**Audience:** developer overview of what the agentic pipeline can do now vs. the last stable release cut to `main`.

`main` was cut at **build-23** (spec: compute amplifier upgrade). Since then, **9 agent builds** have merged to `dev` across 20 commits, covering a large scope of new game mechanics, pipeline hardening, and auth infrastructure.

---

#### What the agents built (unprompted, from spec alone)

**Build 24–25 — Robot economy rebalance**
Agents refactored the robot cost formula from exponential to linear (`10 + 2n`), updated all hardcoded test values across existing compliance tests, and wrote new tests for the new formula. This was the first large refactor run — it required the agents to both fix stale tests and add new ones in the same pass.

**Build 26–29 — Stasis pod death cascade + scrap boost**
Agents implemented escalating pod death logic (1st power outage → -1 pod, 2nd → -5, 3rd+ → -10), a `game_over` condition when all humans are dead, and a scrap boost click mechanic with compute cost and decay. The test suite for this alone is 374 lines covering edge cases like timer reset between expiry events and the interaction between pod count and stasis drain cap.

**Build 32 — Phase compliance hardening**
The agents wrote a 237-line compliance test suite verifying that Phase 1 and Phase 2 gate transitions fire at exactly the right thresholds, and that no Phase 2 functionality is accessible before the gate opens. Not from a new spec feature — proactively from a refactor pass auditing what was underspecified.

**Build 38 — Full Phase 2 implementation**
561-line test file. Agents implemented `awaken_human()`, the human power drain mechanic (`-1.0 power/s per awakened human`), human contribution to scrap rate (`0.5× robot factor`), colony capacity constraints, and the population invariant (`stasis_pod_count + awoken_humans = total living`). All on the first attempt.

**Build 41–42 — Game-over edge cases + balance constants**
Agents self-identified and fixed a post-`game_over` tick processing bug (processing continued after all humans dead). Build 42 was a first-attempt refactor that propagated three simultaneous balance constant changes across both `custodian.cpp` and 6 existing test files.

---

#### Pipeline hardening wins since main

- **Buffet/PAYG auth switching** — pipeline can run on subscription quota or API key with a single command, no Jenkins reconfiguration
- **Spec Diff Agent full qa/ audit** — catches stale tests that don't appear in the diff (implicit numeric staleness from formula changes)
- **Early-exit heuristic** — prevents burning Opus tokens on provably unsatisfiable test patterns; fires correctly on test-setup bugs
- **Artifact recovery** — spec-diff output path mistakes are detected and patched automatically
- **Refactor auto-retry** — failed refactor runs don't update the snapshot, so the pipeline automatically retries the same delta next build

---

#### Rough capacity metric: how much refactor can one build handle?

The best proxy is **lines of agent-generated test code per build** and **number of existing test files touched in one refactor pass**:

| Build | Mode | New test lines | Existing files updated | Notes |
|-------|------|---------------|----------------------|-------|
| 26 | refactor | 374 | 3 | First large refactor |
| 29 | refactor | 423 | 4 | Phase compliance |
| 32 | refactor | 237 | 2 | Proactive hardening |
| 38 | build | 561 | 0 | Full Phase 2, attempt 1 |
| 41 | build | 510 | 0 | Game-over edge cases |
| 42 | refactor | 200 | 6 | 3 constants, 6 stale test files |

**Assessment:** A single build can reliably handle one cohesive feature (300–560 test lines) or a pure balance-constant refactor touching ~6 files. Multi-feature refactors that cross subsystem boundaries (e.g., new struct fields + new tick logic + new public API all at once) are the current limit — build-38 succeeded at that scale on attempt 1 because the spec was tightly written. The `pipeline/quarantine` branch (deferred, not yet merged) is designed to extend this to multi-build iterative refactors.

---

### TODO (pipeline improvements, prioritised)

**Short-term — no new infrastructure:**

1. **Expand early-exit threshold** (Candidate D from build-36 analysis)
   Change identical-failure abort from attempts 2+3 → 3+4. One extra Sonnet attempt before
   Opus escalation. Reduces false-positive aborts on stubborn coding bugs. Low-risk change to
   `Jenkinsfile` coding loop.

2. **Test Agent precondition check**
   Add explicit prompt note: "After writing each test, trace every function call back to its
   preconditions. If the function has a guard (returns false on bad state), verify setup sets
   the guard condition before the call." Addresses the build-28/31 setup-error failure mode.

3. **Snapshot validation on write**
   On `post { success }`, after copying spec snapshot, write a SHA256 checksum alongside it.
   `detect-mode.sh` verifies checksum before trusting snapshot. Catches corruption silently.

**Medium-term — light infrastructure:**

4. **Coding Agent timeout safety**
   On timeout (exit 124 from `timeout` wrapper), run `git checkout -- src/` before the next
   attempt. Prevents partial writes from a hard-killed agent poisoning the retry context.

5. **Quarantine branch merge**
   `pipeline/quarantine` adds per-build `test-quarantine.json` (stale test list from Spec Diff
   Agent) and splits run-tests.sh results into quarantined/non-quarantined failures. Allows
   large refactors to make partial progress across multiple runs without blocking on the hard
   all-or-nothing test gate. Merge to dev when the next large spec change is ready to validate it.

**Long-term — data-driven:**

6. **Divergent inference: Candidate B or C** (build-36 analysis)
   If divergent-inference failures recur (>2 more times), implement one of:
   - B: Coding agent reads test file before implementing
   - C: Spec Diff Agent produces `invariants.md` for unambiguous side-effect documentation

7. **Spec Alignment Agent (post-planner, pre-test)**
   A lightweight agent inserted after the Planner/Spec Diff Agent and before the Test Agent.
   Inputs: work-order.json + spec/core-spec.md. Task: scan the planned scope for ambiguities,
   unstated invariants, and behaviors that could be inferred differently by two independent
   agents. Outputs: an annotated `alignment-notes.md` (clarifications injected into downstream
   agent prompts) and a risk rating per change item (low/medium/high). High-risk items pause
   the build for human review before test/coding agents run.

   **Goal:** eliminate divergent-inference failures at the source — before tests are written —
   rather than patching spec post-failure. This agent does not modify the spec or work order
   directly; it produces supplementary context that makes implicit behavior explicit for that
   build only.

   **Cost consideration:** must be lightweight (Haiku or Sonnet, short prompt, < 60s timeout).
   No file writes to src/ or qa/. The build should still proceed autonomously for low/medium
   risk items — only high-risk items warrant a pause.

8. **Scope size quantification for pre-work agents**
   Define hard limits on what a single build can accept, enforced by the Planner and Spec Diff
   agents. Candidate metrics:
   - Max new test lines: ~600 (empirical ceiling from build-38)
   - Max stale test files updated in one pass: ~6 (build-42 limit)
   - Max new public API functions: 3
   - Max new GameState fields: 4
   - Risk score: new struct fields (+3), new tick logic branch (+2), new API function (+2),
     formula change (+1 per formula), phase gate change (+3)
   - Build refused (or scoped down) if total risk score exceeds threshold

   The Planner already outputs `completion_estimate`. Extend it to also output a structured
   `scope_metrics` block. If metrics exceed thresholds, the agent self-scopes to a subset and
   notes deferred items for future runs — rather than attempting everything and failing.

   **Note:** "complexity" is intentionally excluded for now — not quantifiable without LLM
   judgment, which adds cost. Revisit if the quantitative limits prove insufficient.

---

### Pipeline failure mode: divergent inference from underspecified spec (build-36)

**What happened:**

Spec introduced `awaken_human()`. The spec said "increments `awoken_humans` by 1" but was
silent on `stasis_pod_count`. The population invariant (`stasis_pod_count + awoken_humans =
total living humans`) logically implies that awakening must also decrement `stasis_pod_count`
— but this was not written down.

Two agents in the same build read the same spec and drew different inferences:
- **Test Agent** inferred the decrement from the invariant and wrote tests that require it
  (a capacity-full test that relied on awoken_humans being tracked against the correct total)
- **Coding Agent** (all 3 attempts) did not make the same inference and implemented
  `awaken_human` without the decrement, and with a wrong capacity comparison operator

Result: tests written in build-36 failed against code written in build-36.
The early-exit heuristic fired ("identical failures on attempts 2 and 3") and aborted.

**The early-exit heuristic produced a false positive.** It correctly detected a repeated
identical failure but misclassified the cause. True test contradictions come from a test
asserting behavior that contradicts the spec. Here, the test was correct — it was a stubborn
coding implementation error that happened to be consistent across all attempts.

**The fix:** spec clarification. `awaken_human` now explicitly states the `stasis_pod_count--`
decrement, the `stasis_pod_count > 0` guard, and uses explicit `awoken_humans >=
(int)colony_capacity.value` wording (not just `>`). The population invariant is restated in
two places. Run the build again with the clarified spec.

**Pattern:** *Two agents working from the same spec draw different inferences about unstated
but logically implied behavior.* The conflict lives **within the same build** (same plan):
the test written this run fails against the code written this run.

This is distinct from the previously documented failure modes:
- ≠ Missing preconditions (test calls gated function without setup — was documented 2026-03-07)
- ≠ Wrong threshold values (test uses wrong numeric setup)
- ≠ Infra artifact path (spec-diff writes to wrong directory)

---

### Long-term solution candidates for divergent-inference failures

**Root cause:** implied spec behavior. Any time a spec says "X happens" without saying "Y
does NOT also happen" or "Y is a consequence of X", agents may diverge.

**Candidate A — Spec completeness discipline (no new pipeline work)**
Before pushing any spec change, verify every function description is closed: all side effects
listed, all invariants stated. If the spec requires deriving behavior from an invariant, restate
the derived behavior explicitly. This is authoring discipline, not engineering.

**Candidate B — Coding agent reads test file before implementing (pipeline change)**
After the test agent writes tests but before the coding loop, pipe the new test file into the
coding agent's context as a direct input (not just error output). The agent would see both the
spec AND the test expectations simultaneously, closing the inference gap.

**Candidate C — Spec Diff Agent produces an explicit "invariants and implications" section**
The Spec Diff Agent already reads the full spec. It could be asked to output a supplementary
`invariants.md` artifact that lists non-obvious consequences of each change ("if X then also
Y"). The coding agent would read this alongside the work order.

**Candidate D — Expand early-exit threshold before Opus escalation**
Currently: identical failures on attempts 2+3 → abort. Change to: identical failures on
attempts 3+4 (add one more Sonnet attempt). More chances for the coding agent to get it right
without the false-positive abort. Low-cost change. Doesn't address root cause.

**Recommended near-term:** A (spec discipline) + D (one extra attempt). These require no
pipeline changes and reduce false-positive aborts. Revisit B and C after 5+ more refactor
builds to see if the pattern recurs frequently enough to justify the engineering.

---

## Session 2026-03-07

### Pipeline failure modes observed (builds 28, 30–31)

Three distinct failure modes surfaced this session:

**1. Spec Diff Agent writes to wrong path (build-30)**
The agent completed its analysis correctly but wrote artifacts to a relative workspace
path (`build-artifacts/build-N/`) instead of the absolute `ARTIFACT_DIR`. The verification
check caught it. Mitigation candidate: add explicit "use the exact absolute path, not a
relative path" instruction above the Step 3 output section in `spec-diff.sh`. Occurred once;
watch for recurrence before investing in a fix.

**2. New compliance test has wrong setup (builds 28, 31)**
The Test Agent wrote a test with missing preconditions — `wake_first_robot` called without
first setting `compute.value >= 10`, so the function silently returned false and the test
failed on a downstream assertion. The coding agent correctly identified the implementation
as spec-compliant and made no changes. Both attempts had identical failures, triggering the
early-exit heuristic. Mitigation candidate: add a prompt note to the test agent to verify
all preconditions are met before calling gated functions (those that return false on bad
state). These tests are never committed so each fresh run rolls the dice again. Could also
consider adding a "compile and dry-run" sanity note to the test agent prompt.

**Common thread:** the coding agent is reliably correct (3-for-3 on implementation in
these builds); failures are infrastructure or test quality issues. The early-exit
heuristic is working well — it correctly aborted before burning the Opus attempt both times.

---

### Large refactor run — robot rebalance, scrap boost, stasis pod death (build-26+)

Spec commit `7192a1e` pushed a multi-feature refactor covering:
- Robot cost formula change (exponential → linear: 10+2n)
- Colony base cost 20 → 200
- New `apply_scrap_boost()` / `scrap_boost_click_delta()` with diminishing-return boost + decay
- New stasis pod death mechanic (`power_deficit_timer`, `STASIS_POD_DEATH_THRESHOLD = 30s`)
- Two new GameState fields (`scrap_boost`, `power_deficit_timer`)
- Updated tick engine (boost decay step, pod death step, new scrap rate formula)

This is the largest spec delta sent to the pipeline so far. May exceed what a single refactor
run can reliably implement. **Priority is keeping a working game over improving the pipeline.**

### Branch isolation decision

Pipeline infrastructure improvements (test quarantine / multi-run refactor support) were designed
and implemented in this session but **not merged to dev**. They live on `pipeline/quarantine`.

Rationale: changing pipeline internals and game spec simultaneously makes failures hard to diagnose.
Rule going forward: `pipeline/*` branches merge to dev only when the game spec is stable.

Once the large spec refactor above passes cleanly, merge `pipeline/quarantine` → `dev` to enable
iterative multi-run refactor support for future large spec changes.

**Revert plan if pipeline cannot handle this:**
- Last known-good spec: `58c0474` (post build-25, all prior features implemented and passing)
- To revert: `git checkout 58c0474 -- spec/core-spec.md && git commit && git push`
- This would re-trigger refactor mode with a "spec reverted" diff — agents would revert
  custodian.cpp/hpp to match. Or just leave the implementation as-is if partial work passes tests.
- Option 2: break the spec commit into smaller pieces and run one feature per build.

**Preferred path:** let the pipeline attempt it. If it fails after 2 runs, revert spec and
reintroduce features one at a time.

---

## Session 2026-03-06

### Project setup
- Repo: https://github.com/TheChantingMachman/TheCustodian
- License: MIT
- Pipeline copied from MyTetris — fully reusable, only REPO_NAME updated
- Stack: C++17, Catch2 v3, raylib 5.5, Jenkins (same instance as MyTetris)

### Concept
An AI caretaker wakes on a dead planet. Bootstraps power, compute, and robot workers
(Phase 1), then builds life support for dormant human population (Phase 2), then
transitions to human reintegration (Phase 3). Milestones surface AI inner-dialogue.
Mechanics express the lore — the click is a processor cycle, not an abstraction.

### Architecture decisions
- Logic layer: `src/custodian.hpp` / `src/custodian.cpp` — pipeline-owned, Catch2-tested
- Renderer: `src/main.cpp` — human-owned, raylib, smoke-tested before merge
- Dialogue/lore is renderer content data — not in logic spec
- Same TDD boundary as MyTetris: only unit-testable code goes through the pipeline

### Known issues / TODO
- [x] Create Jenkins job for TheCustodian — done, pipeline running since build-1
- [x] Add `github-repo-url` credential for TheCustodian in Jenkins — done
- [x] Renderer: implement basic raylib UI for Phase 1 — done (build-23 era)
- [x] Install build deps on dev machine — done
- [x] Pipeline → dev branch workflow — done (build-23)
- [x] Design upgrade tree spec — compute amplifier added (build-23); full-spec.md removed
- [ ] **Write Phase 2 spec** — Phase 1 at ~0.85 completion; ready to start designing Phase 2
- [ ] **Release build script** — write `scripts/build-release.sh` and a short `BUILDING.md`
      with dead-simple instructions (install deps, run script, launch binary).
- [ ] **Merge `pipeline/quarantine` → dev** — large spec refactor now stable (build-26+); safe to merge

---

## Session 2026-03-05

### Phase 1 spec redesign
Overhauled the Phase 1 mechanics to make lore and loop coherent:
- Click action now converts power → compute (the AI "thinks harder")
- Robots scavenge ruins for `tech_scrap`; scrap rate boosted by current compute level
- Compute is ephemeral — consumed by robots each tick, resets to 0 (accumulates freely with no robots online)
- Solar panels bought with `tech_scrap`, providing passive power
- Wake-first-robot replaces robot_unit producer: one-time action, costs 10 compute, only available while compute is accumulating (no robots yet)
- Added `ConditionField` enum to Milestone struct to support rate-based triggers (`solar_online` fires on `power.rate >= 0.6`)
- Phase 1→2 gate is a placeholder pending self-sufficiency challenge design

### Pipeline limitations surfaced — notes for future work

**Spec-change problem (do not solve prematurely — encounter it first):**
The pipeline's qa/ regression suite is append-only. This works when the spec only grows.
When a spec change modifies existing behavior (artistic/design decision), old qa/ tests assert
the old behavior and new tests assert the new — the Coding Agent cannot satisfy both. The
pipeline's existing identical-failure heuristic will detect this and abort, but cannot resolve it.

Candidate solution when ready to build: a **Test Validator** stage between Test Agent and Coding Loop.
Reads current spec/ and full qa/, removes or rewrites tests that now contradict the spec. Would
make the pipeline: `Planner → Test Agent → Test Validator → Coding Loop → Closeout`.
The Jenkinsfile already has a TODO comment anticipating this stage.
Verdict: encounter the problem on a real spec change first before locking in the design.

**Refactoring:**
Two distinct refactor needs identified:
1. *Periodic cleanup* — agent-generated code accumulates; needs occasional structural tidying
2. *Spec-driven refactor* — see Refactor Pipeline design below

Plan: let the codebase build out further before designing a refactor pipeline. Real accumulated
agent code will make the requirements clearer.

---

## Session 2026-03-06 (continued)

### Test Validator stage added (build-triggered)
A Test Validator agent was added between Test Agent and Coding Loop after a spec-change build
failure. It reads spec/ and all qa/ files, fixes or removes tests that now contradict the spec,
and writes a validation-report.json artifact. Runs every build; no-op when nothing is stale.
The current Validator handles both the spec-change remediation and wrong-test detection jobs.
See refactor pipeline design below for planned replacement of the spec-change job.

### Refactor pipeline design (not yet built — design agreed, implement when ready)

**Problem:** The Test Validator runs on every build and handles two distinct jobs:
1. Fix stale tests when spec changes (expensive, only needed on spec-change builds)
2. Catch wrong/contradictory tests (useful, but current approach is overkill)

**Agreed design:**

Pipeline runs in two modes: `build` (normal) and `refactor` (spec changed).

Mode is detected cheaply via shell diff — no LLM:
- Closeout writes `spec-snapshot.md` to a stable Jenkins path on successful builds only
- Each build diffs current `spec/core-spec.md` against the snapshot
- If changed → `refactor` mode; if unchanged → `build` mode
- Snapshot is only written on success, so a failed refactor build re-triggers refactor on the next run automatically
- Manual override via Jenkins `PIPELINE_MODE` parameter (auto / build / refactor)
- **Error default: any edge case (no snapshot yet, corrupted file, diff failure) defaults to `refactor`**
  — a refactor run on a clean codebase is just a slightly more expensive build that self-corrects;
  far better than a hard pipeline failure

**Refactor mode pipeline:**
```
Checkout → Spec Diff Agent → Test Agent → Compile Gate → Coding Loop → Closeout
```
- Planner is skipped — the Spec Diff Agent IS the planner for refactor runs
- Spec Diff Agent: reads git diff of spec/, interprets what changed semantically,
  produces `work-order.json` (refactor scope for Coding Agent) + `spec-diff.json` (audit artifact)
  and `test-update-instructions` for the Test Agent
- Test Agent: fixes stale tests (from spec-diff) + writes new tests (from work order)
- Compile Gate: shell-only, no LLM — compiles qa/ against headers, fails fast on bad signatures

**Build mode pipeline (unchanged):**
```
Checkout → Planner → Test Agent → Compile Gate → Coding Loop → Closeout
```
- Planner does discovery as normal
- Compile Gate runs here too (replaces the heavyweight Test Validator for wrong-test detection)

**Large refactor limitation (known, deferred):**
Refactors too large to complete in a single pipeline run will leave tests failing with
certainty since a partial refactor breaks both old and new tests. No solution designed yet —
encounter this in practice before solving it.

---

### Pipeline observability (not yet built — start conservative, expand when data exists)

**Goal:** identify where the pipeline struggles and where it's strong — without building
infrastructure before there's enough data to make it useful.

**Phase 1 — per-build `build-meta.json` (implement when builds are stable)**

A shell script in Closeout assembles a structured summary from existing artifacts:
```json
{
  "build": 12,
  "mode": "refactor",
  "scope_complexity": "moderate",
  "attempts_used": 2,
  "escalated": false,
  "tests_added": 4,
  "tests_fixed": 3,
  "tests_removed": 1,
  "compile_gate": "pass",
  "failure_categories": ["logic_error", "logic_error"],
  "failed_tests": ["test_stasis_drain_growth", "test_colony_cost"],
  "completion_estimate": 0.65,
  "duration_seconds": 847
}
```
Most fields are already in `qa-report.json` and `spec-diff.json` — just extraction and assembly.
No new agents, no new LLM calls. Archive alongside other build artifacts.

**Phase 2 — cross-build `trend-report.sh` (implement after ~10 builds of data)**

Reads all `build-meta.json` files, outputs:
- Attempt count trend (is the pipeline improving?)
- Recurring failing tests across builds (heatmap by test name)
- Failure category breakdown (compile vs logic vs timeout)
- Spec coverage trajectory (`completion_estimate` over time)
- Refactor trigger frequency

**What's not worth building yet:**
- Real-time dashboards — Jenkins history is sufficient until 20+ builds exist
- Per-agent token usage — requires API instrumentation not currently in place
- Scope accuracy scoring (did coding agent implement everything scoped?) — needs a
  closeout validator agent, too heavy for now
