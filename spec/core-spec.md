# Custodian — Core Spec

> C++17, compiled as a single `custodian` executable.

---

<!-- DEVELOPMENT NOTE — for Claude Code (pair programmer), not for pipeline agents

Ownership boundaries in this repo:

  Claude Code (me) owns:
    - spec/          — spec authorship and balance tuning
    - Pipeline/      — Jenkinsfile, agent scripts, utility scripts
    - src/renderer/  — renderer and any future human-facing display code

  Pipeline agents own:
    - src/custodian.hpp / src/custodian.cpp  — game logic implementation
    - qa/                                     — Catch2 test suite

I should read the full repo freely, but should not modify pipeline-agent-owned files
unless the user explicitly asks for a manual intervention. Changes to spec/ or Pipeline/
are the right lever — agents respond to those on the next build run.

Pipeline agents: ignore this comment block entirely.
-->

---

## Premise

An AI wakes on a dead planet. Its directives: bootstrap its own resources, sustain itself,
and prepare the environment for a dormant human population in stasis pods. The player is
the AI. The game is the AI's work.

---

## Scope

This spec covers two layers:

**Layer 1 — Game logic library** (`src/custodian.hpp` / `src/custodian.cpp`):
- Resource model and tick engine
- Click action (power → compute)
- Robot wake mechanic (one-time) + additional robot purchase system
- Robot capacity expansion (Phase 2)
- Solar panel purchase system with degradation and panel cap
- Tech scrap vault upgrades (storage expansion)
- Stasis tax system (growing power drain; escalating pod deaths)
- Lose condition (all humans dead → `game_over`)
- Colony unit purchase system (Phase 2 cap: 5 units / 10 humans)
- Phase 2 human awakening system (progressive power cost per human)
- Phase gate system
- Milestone/event system
- Save/load state
- Phase 3: fabricator + reactor (stubs; balance TBD)

**Layer 2 — Renderer** (`src/renderer/main.cpp`):
- raylib windowed display
- Click input and UI interaction
- Placeholder visuals: text labels, resource readouts, buttons, minimal images
- Human-owned, not pipeline-tested

### Testing boundary

- **Pipeline-owned:** `src/custodian.hpp` / `src/custodian.cpp` — all logic, Catch2-tested
- **Human-owned:** `src/renderer/main.cpp` — rendering, not unit-testable
- Human smoke tests renderer before merging any PR touching `src/renderer/`

### Build

```
g++ -std=c++17 -O2 -I src/ -o custodian src/renderer/main.cpp src/custodian.cpp -lraylib -lm -lpthread -ldl
```

Requires raylib 5.5.

---

## Resources

A resource is a named quantity with a current value, a cap, and a net rate.

```cpp
struct Resource {
    std::string id;       // unique key
    double      value;    // current amount (clamped to [0, cap])
    double      cap;      // maximum storable amount
    double      rate;     // net rate per second (set by recompute_rates or game_tick)
};
```

Resources are stored in `GameState` as a map keyed by id.

---

## Phase 1 Resources

| ID               | Initial value | Initial cap | Initial rate | Notes |
|------------------|--------------|-------------|--------------|-------|
| `power`          | 15.0         | 100.0       | 0.1          | Joules/sec; net of all drains |
| `compute`        | 0.0          | 50.0        | 0.0          | Ephemeral; from clicks; consumed by robots each tick |
| `robots`         | 0.0          | 5.0         | 0.0          | Scavenger count; one-time wake |
| `tech_scrap`     | 0.0          | 500.0       | 0.0          | Salvage; primary currency |
| `stasis_drain`   | 0.05         | 100.0       | 0.0          | Read-only display; value = current stasis power drain/s |
| `colony_capacity`| 0.0          | 50.0        | 0.0          | Housing capacity for awakened humans; built from tech_scrap |

> `stasis_drain` and `colony_capacity` have `rate = 0` always. Their values are set
> directly — `stasis_drain` by the tick engine each frame, `colony_capacity` by
> `buy_colony_unit`. They are resources so the milestone/gate systems can check them uniformly.

---

## Click Action

`player_click(GameState& state)` — the primary player input.

- Costs `click_cost` power. If `power.value < click_cost`, returns 0.0 and does nothing.
- Adds `click_power` compute to `compute.value` (clamped to cap).
- Returns the amount of compute actually added.

| Parameter     | Initial value |
|---------------|--------------|
| `click_power` | 1.0          |
| `click_cost`  | 1.0          |

---

## Wake First Robot

`wake_first_robot(GameState& state)` — one-time action to bring the first autonomous unit online.

- Only available if `first_robot_awoken == false`
- Requires `compute.value >= WAKE_ROBOT_COMPUTE_COST` (10.0)
- Deducts 10.0 compute
- Sets `robots.value = 1.0`
- Sets `first_robot_awoken = true`
- Calls `recompute_rates(state)`
- Returns `false` if unavailable or already done

With no robots online, compute accumulates freely from clicks (nothing consumes it), making
the 10-compute threshold achievable from the start.

---

## Additional Robots

`buy_robot(GameState& state)` — purchase an additional scavenger robot with tech_scrap.

- Only available if `first_robot_awoken == true` and `robots.value < robots.cap` (5.0)
- Checks `tech_scrap.value >= current_robot_cost(state)`
- Deducts cost, increments `robots.value` by 1.0 (clamped to `robots.cap`)
- Calls `recompute_rates(state)`
- Returns `false` if unavailable or cannot afford

`current_robot_cost(state)` = `ROBOT_BASE_COST + ROBOT_COST_STEP * (robots.value - 1.0)`

> Cost is based on current robot count before the purchase. Linear scale.
> Example costs: 2nd robot = 10, 3rd = 12, 4th = 14, 5th = 16 (tech_scrap)

| Constant          | Value |
|-------------------|-------|
| `ROBOT_BASE_COST` | 10.0  |
| `ROBOT_COST_STEP` | 2.0   |

Each additional robot increases passive scrap generation and adds `ROBOT_POWER_DRAIN` (0.2/s)
to power consumption. At maximum capacity (5 robots), total robot drain = 1.0/s — the player
must have sufficient solar generation to support a full workforce.

---

## Compute Scrap Boost

`apply_scrap_boost(GameState& state)` — spend compute to temporarily amplify robot scrap
recovery rate.

- Requires `compute.value >= SCRAP_BOOST_COMPUTE_COST` (5.0)
- Deducts cost
- Adds `scrap_boost_click_delta(state)` to `state.scrap_boost`
- Returns `false` if insufficient compute

`scrap_boost_click_delta(state)` = `SCRAP_BOOST_BASE / (1.0 + state.scrap_boost / SCRAP_BOOST_BASE)`

> When `scrap_boost == 0`, each click adds `SCRAP_BOOST_BASE` (3%). As boost accumulates,
> each successive click adds less. As the boost decays back toward 0, click power recovers
> proportionally. This creates a rhythm: click during low-boost windows for maximum return.

`scrap_boost` decays linearly each tick:
```
scrap_boost = max(0.0, scrap_boost - SCRAP_BOOST_DECAY * delta_seconds)
```

| Constant                   | Value | Notes |
|----------------------------|-------|-------|
| `SCRAP_BOOST_COMPUTE_COST` | 5.0   | compute per click |
| `SCRAP_BOOST_BASE`         | 0.06  | base boost per click at 0 boost (6%) |
| `SCRAP_BOOST_DECAY`        | 0.002 | boost lost per second (linear) |

`scrap_boost` is a field in `GameState` (starts `0.0`). It affects the scrap rate multiplier
(see Dynamic Scrap Rate).

---

## Dynamic Scrap Rate

`tech_scrap` rate is computed each tick before resources are advanced:

```
effective_workers = robots.value + awoken_humans * HUMAN_SCRAP_FACTOR
tech_scrap.rate   = effective_workers * (BASE_SCRAP_RATE + compute.value * COMPUTE_SCRAP_BONUS)
                  * (1.0 + state.scrap_boost)
```

Awakened humans contribute to scrap recovery at half the rate of a robot — they are organic
labour, slower but free of the power-per-robot overhead.

| Constant              | Value |
|-----------------------|-------|
| `BASE_SCRAP_RATE`     | 0.1   | tech_scrap per effective worker per second |
| `COMPUTE_SCRAP_BONUS` | 0.02  | additional tech_scrap/s per point of compute (flat) |
| `HUMAN_SCRAP_FACTOR`  | 0.5   | fraction of robot scrap rate contributed per human |

> Example: 1 robot + 2 humans (= 2.0 effective workers), 5 compute → (0.1 + 0.10) × 2.0 = 0.4/s
> Example: 5 robots, 20 compute → (0.1 + 0.40) × 5.0 = 2.5/s

> **Emergent mechanic (intentional, under observation):** The current power level acts as an
> additional multiplier on the scrap boost, creating an incentive to run power close to zero
> for maximum scrap throughput. This is a known side-effect of the resource clamping interaction,
> not a designed feature. It produces interesting risk/reward gameplay and is preserved
> intentionally for now. It may be balanced or codified as an explicit mechanic in a future spec.

---

## Solar Panels

`buy_solar_panel(GameState& state)` — spend tech_scrap to increase power generation.

- Returns `false` if `solar_panel_count >= SOLAR_PANEL_MAX` (panel cap reached)
- Checks `tech_scrap.value >= current_solar_cost(state)`
- Deducts cost, increments `state.solar_panel_count`
- Calls `recompute_rates(state)`
- Returns `false` if cannot afford

`current_solar_cost(state)` = `SOLAR_BASE_COST * pow(SOLAR_COST_SCALE, solar_panel_count)`

| Constant            | Value | Notes |
|---------------------|-------|-------|
| `SOLAR_BASE_COST`   | 5.0   | cost of first panel |
| `SOLAR_COST_SCALE`  | 1.05  | exponential cost multiplier per panel (was 1.10) |
| `SOLAR_POWER_RATE`  | 0.75  | power/sec added per functional panel (was 0.5) |
| `SOLAR_PANEL_MAX`   | 20    | hard cap on total panels purchasable |
| `SOLAR_DECAY_RATE`  | 0.002 | fractional panels lost per second at maximum density (was 0.005) |

### Solar Panel Degradation

Solar panels degrade over time. Degradation rate scales with how close the array is to capacity —
a crowded array is harder to maintain. Lost panels reduce power output permanently until replaced.

Each tick, a degradation accumulator advances:
```
solar_decay_accumulator += SOLAR_DECAY_RATE * (solar_panel_count / SOLAR_PANEL_MAX) * delta_seconds
```

When `solar_decay_accumulator >= 1.0`:
- `solar_panel_count -= 1` (one panel lost; clamped to 0)
- `solar_decay_accumulator -= 1.0`
- Push `"panel_degraded"` onto `state.pending_events`

> At maximum density (20 panels), degradation fires every 250 seconds — one panel lost roughly
> every 4 minutes. At half density (10 panels), every 500 seconds. Players must keep buying
> panels to maintain output, creating an ongoing resource pressure.

`solar_decay_accumulator` is a field in `GameState` (starts `0.0`).

---

## Stasis Tax

The stasis pods housing the dormant population draw supplementary power. Cell degradation
causes this draw to grow over time, creating escalating pressure on the player's power budget.

`stasis_drain` is updated at the start of each tick:

```
stasis_drain_value = min(
    STASIS_DRAIN_BASE + STASIS_DRAIN_GROWTH * total_time,
    STASIS_POD_COUNT * STASIS_DRAIN_PER_POD,
    STASIS_DRAIN_CAP
)
state.resources["stasis_drain"].value = stasis_drain_value
```

| Constant               | Value | Notes |
|------------------------|-------|-------|
| `STASIS_DRAIN_BASE`    | 0.05  | initial drain at t=0 |
| `STASIS_DRAIN_GROWTH`  | 0.001 | drain increase per second of total_time (was 0.003) |
| `STASIS_DRAIN_PER_POD` | 2.0   | max drain per pod |
| `STASIS_DRAIN_CAP`     | 4.0   | hard ceiling on total stasis drain regardless of time or pod count |
| `STASIS_POD_COUNT`     | 50    | fixed number of pods (constant, also stored in GameState) |

Maximum stasis drain = **4.0 power/s** (hard cap). At 0.001/s growth, the cap is reached
at t ≈ 3,950 seconds (~66 min). At t ≈ 950 seconds (~16 min), drain reaches 1.0/s —
the `stasis_warning` milestone threshold.

> Stasis drain is intentionally capped so that 20 solar panels (15.0/s at SOLAR_POWER_RATE 0.75)
> can sustain up to ~6 awakened humans (7.5/s) plus max stasis drain (4.0/s), leaving a small
> positive margin. Players must awaken colonists and advance phases before the reactor makes
> large human populations viable.

This drain is included in `power.rate` via `recompute_rates` (called each tick after the
drain value is updated). The player must expand solar generation to stay net-positive.

---

## Stasis Pod Death

If the station's power supply is exhausted for too long, stasis pods begin failing. Each lost
pod permanently reduces the maximum stasis drain (fewer pods to maintain) but also permanently
reduces the human population — those colonists are unrecoverable.

`power_deficit_timer` accumulates while `power.value <= 0`:
```
if power.value <= 0:
    power_deficit_timer += delta_seconds
else:
    power_deficit_timer = 0.0
```

When `power_deficit_timer >= STASIS_POD_DEATH_THRESHOLD` and `stasis_pod_count > 0`:

1. Compute pods to lose based on how many expiry events have occurred so far (`pod_death_count`):
   - `pod_death_count == 0` (first expiry): lose **1** pod
   - `pod_death_count == 1` (second expiry): lose **5** pods
   - `pod_death_count >= 2` (third+ expiry): lose **10** pods
   - Clamp to `stasis_pod_count` (cannot lose more pods than remain)
2. `stasis_pod_count -= pods_to_lose`
3. `pod_death_count += 1`
4. Reset `power_deficit_timer = 0.0`
5. Push `"pod_lost"` onto `state.pending_events`
6. If `stasis_pod_count + state.awoken_humans == 0`: set `game_over = true`, push `"game_over"`

The stasis drain formula already uses `stasis_pod_count` as the cap multiplier, so each lost
pod automatically reduces both the maximum possible drain and the current drain if at the cap.

| Constant                      | Value | Notes |
|-------------------------------|-------|-------|
| `STASIS_POD_DEATH_THRESHOLD`  | 30.0  | seconds at zero power before a pod death event |

`power_deficit_timer` and `pod_death_count` are fields in `GameState` (both start `0.0` / `0`).

> Population invariant: `stasis_pod_count` tracks humans still in stasis pods (decremented by pod
> death events AND by `awaken_human`). `awoken_humans` tracks humans in the colony. Total living
> population = `stasis_pod_count + awoken_humans`. When this reaches 0, `game_over = true`.

> When `game_over == true`, all humans are dead. No further pod deaths or awakenings can occur.
> The renderer should display a game-over screen. `game_tick` must not process further updates
> when `game_over == true`.

---

## Colony Units

`buy_colony_unit(GameState& state)` — build infrastructure to receive awakened humans.

- Checks `tech_scrap.value >= current_colony_cost(state)`
- Deducts cost, increments `colony_unit_count`
- Adds `COLONY_CAPACITY_PER_UNIT` to `colony_capacity.value` (clamped to cap)
- Returns `false` if cannot afford

`current_colony_cost(state)` = `COLONY_UNIT_BASE_COST * pow(COLONY_UNIT_COST_SCALE, colony_unit_count)`

| Constant                  | Value |
|---------------------------|-------|
| `COLONY_UNIT_BASE_COST`   | 200.0 |
| `COLONY_UNIT_COST_SCALE`  | 1.20  |
| `COLONY_CAPACITY_PER_UNIT`| 2     |

Colony capacity represents housing and life-support slots for awakened humans. Each shelter
unit supports 2 humans. Humans cannot be safely awakened (Phase 2) without a free capacity
slot (`colony_capacity.value > awoken_humans`). The first unit costs 200 tech_scrap;
subsequent units scale at ×1.20 per purchase.

### Phase 2 Colony Cap

In Phase 2, `buy_colony_unit` returns `false` if `colony_unit_count >= COLONY_PHASE2_MAX_UNITS`.
This limits Phase 2 colony to 5 shelters (10 humans maximum). Phase 3 removes this restriction.

| Constant                  | Value | Notes |
|---------------------------|-------|-------|
| `COLONY_PHASE2_MAX_UNITS` | 5     | max shelter units purchasable in Phase 2 (10 human slots) |

> Population invariant still holds: `stasis_pod_count + awoken_humans` = total living at all times.
> The colony cap constrains awakening, not the total population.

---

## Awaken Human (Phase 2)

`awaken_human(GameState& state)` — bring a dormant colonist out of stasis and into the colony.

- Returns `false` immediately if `current_phase < 2`
- Returns `false` immediately if `stasis_pod_count <= 0` (no humans left in stasis to wake)
- Returns `false` immediately if there is no free colony slot: `(int)colony_capacity.value <= awoken_humans`
  - Note: this check uses **strictly less-than-or-equal**, i.e. succeeds only when `awoken_humans < (int)colony_capacity.value`
- Decrements `stasis_pod_count` by 1 (human leaves the stasis pod — pod count tracks occupied pods)
- Increments `awoken_humans` by 1
- Calls `recompute_rates(state)` — adds `HUMAN_POWER_DRAIN` to power consumption
- Pushes `"human_awakened"` onto `state.pending_events` on success
- Returns `true` on success

Population invariant: `stasis_pod_count + awoken_humans` equals the total living human population at all
times. Pod death events reduce `stasis_pod_count` (humans die). Awakening moves a human from stasis to
colony (`stasis_pod_count--`, `awoken_humans++`), preserving the total.

Each awakened human draws power and contributes to scrap recovery (see Dynamic Scrap Rate).
The player must ensure power stays positive or the stasis pod death cascade accelerates.

---

## Compute Amplifier

`buy_compute_amplifier(GameState& state)` — one-time upgrade that expands the compute buffer.

- Only available if `compute_amplified == false`
- Requires `tech_scrap.value >= COMPUTE_AMP_COST` (40.0)
- Deducts cost, sets `compute.cap = COMPUTE_AMP_CAP` (150.0), sets `compute_amplified = true`
- Pushes `"compute_amplified"` onto `state.pending_events` on success
- Returns `false` if already done or cannot afford

| Constant           | Value | Notes |
|--------------------|-------|-------|
| `COMPUTE_AMP_COST` | 40.0  | tech_scrap cost |
| `COMPUTE_AMP_CAP`  | 150.0 | compute cap after upgrade (was 50.0) |

Expanding the compute buffer lets the robot accumulate more `COMPUTE_SCRAP_BONUS` value between
ticks, increasing scrap throughput. Available once enough scrap has been salvaged.

---

## Tech Scrap Vault

Two one-time upgrades that expand the tech_scrap storage cap. Each doubles the cap.

`buy_scrap_vault(GameState& state)` — purchase the next available vault tier.

- Returns `false` if `tech_scrap_vault_tier >= 2` (both tiers already purchased)
- Requires `tech_scrap.value >= vault_cost` (see table below)
- Deducts cost, doubles `tech_scrap.cap`, increments `tech_scrap_vault_tier`
- Pushes `"vault_expanded"` onto `state.pending_events`
- Returns `true` on success

| Tier | Requires vault_tier | Cost (tech_scrap) | New cap |
|------|--------------------|--------------------|---------|
| 1    | 0                  | 300.0              | 1000.0  |
| 2    | 1                  | 800.0              | 2000.0  |

> Vault costs are deducted from tech_scrap before the cap is expanded. The purchase must be
> affordable at the current cap — if tech_scrap.value is already at the old cap limit, the
> player must spend down before buying the next tier is useful.

`tech_scrap_vault_tier` is a field in `GameState` (starts `0`).

---

## Robot Capacity Expansion (Phase 2)

`buy_robot_capacity(GameState& state)` — permanently expand the robot workforce cap from 5 to 10.

- Returns `false` if `current_phase < 2`
- Returns `false` if `robots_cap_expanded == true` (already purchased)
- Returns `false` if `robots.value < robots.cap` (must have all current robots active first)
- Requires `tech_scrap.value >= ROBOT_CAP_EXPAND_COST` (500.0)
- Deducts cost, sets `robots.cap = 10.0`, sets `robots_cap_expanded = true`
- Calls `recompute_rates(state)`
- Pushes `"robot_cap_expanded"` onto `state.pending_events`
- Returns `true` on success

| Constant                | Value |
|-------------------------|-------|
| `ROBOT_CAP_EXPAND_COST` | 500.0 |

> After expansion, `buy_robot` continues to use the same linear cost formula based on current
> `robots.value`. Robots 6–10 follow the same `ROBOT_BASE_COST + ROBOT_COST_STEP * (n-1)` scale.

`robots_cap_expanded` is a field in `GameState` (starts `false`).

---

## Tick Engine

`game_tick(GameState& state, double delta_seconds)` — called every frame:

0. If `game_over == true`: return immediately (no further processing)
1. Update stasis drain: `resources["stasis_drain"].value = min(STASIS_DRAIN_BASE + STASIS_DRAIN_GROWTH * total_time, stasis_pod_count * STASIS_DRAIN_PER_POD, STASIS_DRAIN_CAP)`
2. Call `recompute_rates(state)` — rebuilds all static rates including the updated stasis drain
3. Decay scrap boost: `scrap_boost = max(0.0, scrap_boost - SCRAP_BOOST_DECAY * delta_seconds)`
4. Set dynamic scrap rate: `tech_scrap.rate = (robots.value + awoken_humans * HUMAN_SCRAP_FACTOR) * (BASE_SCRAP_RATE + compute.value * COMPUTE_SCRAP_BONUS) * (1.0 + scrap_boost)`
5. Advance all resources: `value = clamp(value + rate * delta, 0.0, cap)`
   - `stasis_drain` and `colony_capacity` have rate=0, so their values set in steps 1/buy are preserved
5a. Apply solar panel degradation:
   - `solar_decay_accumulator += SOLAR_DECAY_RATE * (solar_panel_count / SOLAR_PANEL_MAX) * delta_seconds`
   - While `solar_decay_accumulator >= 1.0` and `solar_panel_count > 0`:
     - `solar_panel_count -= 1`
     - `solar_decay_accumulator -= 1.0`
     - Push `"panel_degraded"` onto `state.pending_events`
     - Call `recompute_rates(state)` (power rate changes immediately)
   - If `solar_panel_count == 0`: `solar_decay_accumulator = 0.0` (no panels to degrade)
6. Update power deficit timer:
   - If `power.value <= 0.0`: `power_deficit_timer += delta_seconds`
   - Else: `power_deficit_timer = 0.0`
   - If `power_deficit_timer >= STASIS_POD_DEATH_THRESHOLD` and `stasis_pod_count > 0`:
     - Compute `pods_to_lose`: 1 if pod_death_count==0, 5 if pod_death_count==1, 10 otherwise
     - `pods_to_lose = min(pods_to_lose, stasis_pod_count)`
     - `stasis_pod_count -= pods_to_lose`
     - `pod_death_count += 1`
     - Reset `power_deficit_timer = 0.0`
     - Push `"pod_lost"` event
     - If `stasis_pod_count + awoken_humans == 0`: set `game_over = true`, push `"game_over"`
7. Call `check_phase_gates(state)`
8. Call `check_milestones(state)`
9. `total_time += delta_seconds`

---

## Rate Recomputation

`recompute_rates(GameState& state)` — called after any purchase, wake action, or each tick.

Rebuilds all resource `rate` fields:

```
int    n                = awoken_humans;
double human_power_cost = n * HUMAN_POWER_BASE + HUMAN_POWER_SCALE * n * (n - 1) / 2.0;

power.rate       = BASE_POWER_RATE
                 + solar_panel_count * SOLAR_POWER_RATE
                 - robots.value * ROBOT_POWER_DRAIN
                 - human_power_cost
                 - resources["stasis_drain"].value

compute.rate     = -robots.value * ROBOT_COMPUTE_DRAIN   (negative when robots active; 0.0 otherwise)
robots.rate      = 0.0
tech_scrap.rate  = 0.0   (set dynamically in game_tick)
stasis_drain.rate = 0.0  (value set directly in game_tick)
colony_capacity.rate = 0.0  (value set by buy_colony_unit)
```

> Compute is no longer reset to 0 each tick. Instead, active robots drain the compute buffer
> at `ROBOT_COMPUTE_DRAIN` per robot per second. The player accumulates compute via clicks and
> spends it on `apply_scrap_boost`. With 5 robots draining 2.5/s and clicks giving 1.0 each,
> the player can accumulate a compute buffer more easily than before.

| Constant              | Value | Notes |
|-----------------------|-------|-------|
| `BASE_POWER_RATE`     | 0.1   | degraded solar array (always present) |
| `ROBOT_POWER_DRAIN`   | 0.2   | power/sec per active robot |
| `ROBOT_COMPUTE_DRAIN` | 0.5   | compute/sec drained per active robot |
| `HUMAN_POWER_BASE`    | 0.5   | power/sec cost of the first awakened human (was 1.0) |
| `HUMAN_POWER_SCALE`   | 0.3   | additional power/sec cost per additional human beyond the first (was 0.5) |

Human power cost is progressive. Each newly awakened human draws more power than the previous:
- 1st human: 0.5/s total cumulative
- 2nd human: +0.8/s → 1.3/s total
- 3rd human: +1.1/s → 2.4/s total
- nth human adds `HUMAN_POWER_BASE + HUMAN_POWER_SCALE * (n-1)` to the total drain

Closed-form total: `n × HUMAN_POWER_BASE + HUMAN_POWER_SCALE × n × (n-1) / 2`

> At 5 humans: 5.5/s total human drain. At 10 humans: 18.5/s. Awakening decisions are
> irreversible and power-impactful — each one permanently shifts the power budget.

---

## Phase Gate System

```cpp
struct GateCondition {
    std::string    resource_id;
    double         min_value;
    ConditionField condition_field;   // VALUE (default) or RATE
};

struct PhaseGate {
    int                          phase;
    std::vector<GateCondition>   conditions;
};
```

`check_phase_gates(GameState& state)` — called each tick. For the gate whose `phase ==
current_phase + 1`, if all conditions are met, advance `state.current_phase`.

Condition check mirrors the Milestone system:
- `VALUE`: `resources[resource_id].value >= min_value`
- `RATE`:  `resources[resource_id].rate  >= min_value`

### Phase 1 → Phase 2 gate

| Resource          | Field | Min value | Meaning |
|-------------------|-------|-----------|---------|
| `power`           | RATE  | 0.0       | Net power generation is non-negative (self-sufficient) |
| `colony_capacity` | VALUE | 2.0       | At least one colony unit built; ready to receive humans |

The player must simultaneously sustain the stasis tax via solar expansion AND build at least
one colony shelter before Phase 2 unlocks.

### Phase 2 → Phase 3 gate

| Resource          | Field | Min value | Meaning |
|-------------------|-------|-----------|---------|
| `colony_capacity` | VALUE | 10.0      | Colony at full Phase 2 capacity (5 shelters, 10 slots) |
| `robots`          | VALUE | 10.0      | Robot workforce fully expanded |

Both conditions must be met simultaneously. The player must have committed fully to both
colony infrastructure and robot expansion before the fabricator and reactor become available.

> Note: meeting these conditions does not require all 10 colony slots to be filled —
> only that the infrastructure (shelters and robots) is in place.

---

## Phase 3 Overview (stubs — balance values TBD)

Phase 3 represents transitioning from survival to permanent settlement. Two major construction
projects gate further progress: the Fabricator and the Reactor.

### Fabricator

A one-time construction purchase using `tech_scrap`. Enables future crafting and colony
expansion beyond Phase 2 limits. Cost and exact effects TBD.

`build_fabricator(GameState& state)` — stub; requires Phase 3, large tech_scrap cost.

- Sets `fabricator_built = true`
- Pushes `"fabricator_online"` onto `state.pending_events`

### Reactor

A one-time construction that permanently resolves most power needs, but requires a large
upfront power storage (capacitor charge) to jump-start. The player must accumulate a power
reserve before triggering construction.

`build_reactor(GameState& state)` — stub; requires Phase 3 and `power.value >= REACTOR_CHARGE_COST`.

- Deducts `REACTOR_CHARGE_COST` from `power.value` (the jump-start draw)
- Adds a large permanent `BASE_POWER_RATE` bonus (flat addition to recompute_rates)
- Sets `reactor_built = true`
- Pushes `"reactor_online"` onto `state.pending_events`

> All Phase 3 constants (`REACTOR_CHARGE_COST`, `FABRICATOR_COST`, reactor power bonus) are
> deferred. This section exists to establish field names and function signatures for the
> GameState and Public API so agents do not invent their own.

`fabricator_built` and `reactor_built` are fields in `GameState` (both start `false`).

---

## Milestone System

```cpp
enum class ConditionField { VALUE, RATE };

struct Milestone {
    std::string    id;
    std::string    condition_resource;
    double         condition_value;
    ConditionField condition_field;   // VALUE (default) or RATE
    bool           triggered;
};
```

`check_milestones(GameState& state)` — called each tick. For each untriggered milestone:
- `VALUE`: fire when `resources[condition_resource].value >= condition_value`
- `RATE`:  fire when `resources[condition_resource].rate  >= condition_value`

Set `triggered = true` and push `id` onto `state.pending_events`.

The renderer drains `pending_events` to display dialogue. The logic layer only produces
event ids.

### Phase 1 milestones

| ID                | Resource          | Field | Threshold | Notes |
|-------------------|-------------------|-------|-----------|-------|
| `first_compute`   | `compute`         | VALUE | 1.0       | First click registered |
| `first_robot`     | `robots`          | VALUE | 1.0       | First autonomous unit online |
| `solar_online`    | `power`           | RATE  | 0.6       | First solar panel bought — rate exceeds base |
| `scrap_flowing`   | `tech_scrap`      | VALUE | 20.0      | Meaningful salvage recovered |
| `stasis_warning`  | `stasis_drain`    | VALUE | 1.0       | Stasis draw exceeds 1.0/s — crisis escalating (~t=317s) |
| `colony_started`  | `colony_capacity` | VALUE | 2.0       | First colony unit complete |
| `all_robots_online` | `robots`        | VALUE | 5.0       | Full robot workforce deployed |

### Cross-phase events (pushed directly, not via milestone system)

| ID                   | Trigger | Notes |
|----------------------|---------|-------|
| `pod_lost`           | `power_deficit_timer` expiry | Pushed each time a pod death event fires |
| `human_awakened`     | `awaken_human()` success | First human out of stasis |
| `game_over`          | `stasis_pod_count + awoken_humans == 0` | All humans dead; game ends |
| `panel_degraded`     | Solar panel degradation tick | Pushed each time a panel is lost |
| `vault_expanded`     | `buy_scrap_vault()` success | Storage upgrade purchased |
| `robot_cap_expanded` | `buy_robot_capacity()` success | Robot cap expanded to 10 |
| `fabricator_online`  | `build_fabricator()` success | Phase 3 fabricator complete |
| `reactor_online`     | `build_reactor()` success | Phase 3 reactor online |

---

## Game State

```cpp
GameState {
    std::map<std::string, Resource>   resources;
    int                               current_phase;       // starts at 1
    std::vector<Milestone>            milestones;
    std::vector<PhaseGate>            phase_gates;
    std::vector<std::string>          pending_events;      // drained by renderer
    double                            total_time;          // seconds elapsed
    double                            click_power;         // compute per click, starts 1.0
    double                            click_cost;          // power per click, starts 1.0
    int                               solar_panel_count;   // starts 0
    int                               colony_unit_count;   // starts 0 (for cost scaling)
    int                               stasis_pod_count;    // starts 50; decremented on pod death events
    int                               awoken_humans;       // starts 0; humans out of stasis in colony
    int                               pod_death_count;     // starts 0; number of pod-death expiry events (for escalation)
    bool                              game_over;           // starts false; set when stasis_pod_count + awoken_humans == 0
    bool                              first_robot_awoken;       // starts false
    bool                              compute_amplified;        // starts false
    bool                              robots_cap_expanded;      // starts false; true after buy_robot_capacity()
    bool                              fabricator_built;         // starts false; Phase 3 stub
    bool                              reactor_built;            // starts false; Phase 3 stub
    double                            scrap_boost;              // scrap rate multiplier bonus, starts 0.0, decays over time
    double                            power_deficit_timer;      // seconds power has been at 0, resets when power > 0
    double                            solar_decay_accumulator;  // fractional panel decay, starts 0.0
    int                               tech_scrap_vault_tier;    // starts 0; max 2
}
```

---

## Public API

```cpp
// Lifecycle
GameState   game_init();
void        game_tick(GameState& state, double delta_seconds);

// Persistence
std::string save_game(const GameState& state);
GameState   load_game(const std::string& data);

// Player actions
double      player_click(GameState& state);               // returns compute added (0 if no power)
bool        wake_first_robot(GameState& state);           // false if already done or compute < threshold
bool        buy_robot(GameState& state);                  // false if first not awoken, at cap, or cannot afford
bool        apply_scrap_boost(GameState& state);          // false if compute < SCRAP_BOOST_COMPUTE_COST
bool        buy_solar_panel(GameState& state);            // false if cannot afford
bool        buy_colony_unit(GameState& state);            // false if cannot afford or Phase 2 cap reached
bool        buy_compute_amplifier(GameState& state);      // false if already done or cannot afford
bool        awaken_human(GameState& state);               // false if phase < 2 or no free colony capacity
bool        buy_scrap_vault(GameState& state);            // false if vault_tier >= 2 or cannot afford
bool        buy_robot_capacity(GameState& state);         // false if phase < 2, already done, not at cap, or cannot afford
bool        build_fabricator(GameState& state);           // false if phase < 3 or already built (stub)
bool        build_reactor(GameState& state);              // false if phase < 3, already built, or power < charge cost (stub)

// Internal (exposed for testing)
double      current_solar_cost(const GameState& state);
double      current_colony_cost(const GameState& state);
double      current_robot_cost(const GameState& state);
double      scrap_boost_click_delta(const GameState& state);
void        recompute_rates(GameState& state);
void        check_phase_gates(GameState& state);
void        check_milestones(GameState& state);
```
