// Build 29 — test_compliance_build29.cpp
// Edge-case compliance tests for the fully-implemented Phase 1 spec.
// Targets gaps not covered by any prior qa/ file.
//
// All values derived from spec/core-spec.md only.
//
// Constants used:
//   BASE_POWER_RATE      = 0.1
//   SOLAR_POWER_RATE     = 0.5
//   ROBOT_POWER_DRAIN    = 0.2
//   STASIS_DRAIN_BASE    = 0.05
//   STASIS_DRAIN_GROWTH  = 0.003
//   STASIS_DRAIN_PER_POD = 0.4
//   STASIS_POD_COUNT     = 50 (default)
//   ROBOT_BASE_COST      = 10.0
//   ROBOT_COST_STEP      = 2.0
//   SCRAP_BOOST_DECAY    = 0.002
//   SCRAP_BOOST_BASE     = 0.03
//   COMPUTE_AMP_COST     = 40.0
//   COMPUTE_AMP_CAP      = 150.0
//   WAKE_ROBOT_COMPUTE_COST = 10.0
//
// Sections:
//   1. Phase gate RATE boundary: power.rate just below 0.0 fails; colony 9.9 fails
//   2. game_tick with 5 robots: recomputed power.rate=-0.95 blocks phase advance
//   3. Phase 2 no re-advance: no phase 3 gate; stays at 2 via game_tick
//   4. Stasis drain=0.0 when pod_count=0; power.rate unaffected by zero drain
//   5. stasis_warning timing: fires at t=317 (drain=1.001), not at t=316 (drain=0.998)
//   6. Scrap boost step ordering: post-decay boost (step 3) is used for rate (step 4)
//   7. Full scrap boost decay: 0.03 → 0.0 in exactly 15 ticks of delta=1.0
//   8. Milestone idempotency via game_tick across multiple ticks
//   9. Integration: wake_first_robot + buy_robot chain 1→5; total scrap cost = 52
//  10. stasis_pod_count=25 caps drain at 1.0 at t=500 (formula yields 1.55)
//  11. compute.cap=150 survives save/load after buy_compute_amplifier
//  12. stasis_warning + colony_started fire in same tick and are idempotent

#include <catch2/catch_all.hpp>
#include <algorithm>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helper: count occurrences of id in pending_events
// ---------------------------------------------------------------------------
static int count_event(const GameState& s, const std::string& id) {
    int n = 0;
    for (const auto& ev : s.pending_events) {
        if (ev == id) n++;
    }
    return n;
}

// ===========================================================================
// Section 1 — Phase gate RATE boundary conditions
//
// Phase 1→2 gate (spec):
//   power RATE >= 0.0  AND  colony_capacity VALUE >= 10.0
// ===========================================================================

TEST_CASE("check_phase_gates: power.rate=-0.001 (just below 0) fails gate even with colony=10.0",
          "[gates][build29]") {
    // power.rate is -0.001 — negative by the smallest amount; gate must fail
    GameState s = game_init();
    s.resources.at("power").rate            = -0.001;
    s.resources.at("colony_capacity").value = 10.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates: colony_capacity=1.9 (just below 2.0) fails gate even with power.rate=0.5",
          "[gates][build29]") {
    // colony_capacity is 1.9 — one tenth below the 2.0 threshold; gate must fail
    GameState s = game_init();
    s.resources.at("power").rate            = 0.5;
    s.resources.at("colony_capacity").value = 1.9;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

// ===========================================================================
// Section 2 — game_tick with 5 robots blocks phase advance
//
// With 5 robots:
//   power.rate = BASE_POWER_RATE - 5*ROBOT_POWER_DRAIN - stasis_drain.value
//              = 0.1 - 1.0 - 0.05 = -0.95  (at t=0)
// Even with colony_capacity=10.0, the negative power.rate prevents the gate.
// ===========================================================================

TEST_CASE("game_tick with 5 robots: recomputed power.rate=-0.95 blocks phase 1→2 gate",
          "[gates][tick][build29]") {
    // Precondition: colony meets value threshold; only power rate fails
    GameState s = game_init();
    s.resources.at("robots").value          = 5.0;
    s.first_robot_awoken                    = true;
    s.resources.at("colony_capacity").value = 10.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.95));
    REQUIRE(s.current_phase == 1);
}

// ===========================================================================
// Section 3 — Phase 2 no re-advance: no phase 3 gate exists
//
// Spec: check_phase_gates looks for a gate whose phase == current_phase + 1.
// With current_phase=2 and no phase 3 gate defined, nothing should advance.
// ===========================================================================

TEST_CASE("game_tick in phase 2 with all conditions exceeded: no phase 3 advance",
          "[gates][tick][build29]") {
    // At t=0, no robots, no panels: power.rate = 0.1 - 0.05 = 0.05 (positive)
    // colony_capacity=50.0 >> 10.0; all gate conditions satisfied but phase 3 has no gate
    GameState s = game_init();
    s.current_phase                         = 2;
    s.resources.at("colony_capacity").value = 50.0;
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("game_tick in phase 2: stays at 2 after two consecutive ticks",
          "[gates][tick][build29]") {
    GameState s = game_init();
    s.current_phase                         = 2;
    s.resources.at("colony_capacity").value = 50.0;
    game_tick(s, 0.0);
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 2);
}

// ===========================================================================
// Section 4 — Stasis drain = 0.0 when pod_count = 0
//
// Spec formula: min(BASE + GROWTH*t, pod_count * PER_POD)
// With pod_count=0: cap = 0 * 0.04 = 0.0 → drain clamped to 0.0
// Consequence: power.rate loses stasis term → power.rate = BASE_POWER_RATE = 0.1
// ===========================================================================

TEST_CASE("game_tick: stasis_drain.value = 0.0 when stasis_pod_count = 0",
          "[stasis][tick][build29]") {
    // formula = 0.05 + 0.003*0 = 0.05; cap = 0*0.04 = 0.0; min(0.05, 0.0) = 0.0
    GameState s = game_init();
    s.stasis_pod_count = 0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.0));
}

TEST_CASE("game_tick: power.rate = 0.1 when stasis_pod_count=0 (no drain, no robots, no panels)",
          "[stasis][tick][build29]") {
    // drain=0.0; power.rate = 0.1 + 0*0.5 - 0*0.2 - 0.0 = 0.1
    GameState s = game_init();
    s.stasis_pod_count = 0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.1));
}

TEST_CASE("game_tick: stasis_drain.value = 0.0 when pod_count=0 at late total_time",
          "[stasis][tick][build29]") {
    // At t=700: formula = 0.05 + 0.003*700 = 2.15; cap = 0*0.04 = 0.0 → 0.0
    // Verifies pod_count=0 clamps regardless of how large the formula result is
    GameState s = game_init();
    s.stasis_pod_count = 0;
    s.total_time       = 700.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.0));
}

// ===========================================================================
// Section 5 — stasis_warning timing via game_tick
//
// stasis_warning milestone: stasis_drain VALUE >= 1.0
// At t=949: drain = 0.05 + 0.001*949 = 0.999 < 1.0  → NOT triggered
// At t=950: drain = 0.05 + 0.001*950 = 1.0 >= 1.0   → TRIGGERED
// total_time is used for step 1 (pre-tick value), then incremented at step 10.
// ===========================================================================

TEST_CASE("game_tick: stasis_drain = 0.366 at total_time=316, stasis_warning NOT fired",
          "[milestone][stasis_warning][build29]") {
    // 0.05 + 0.001*316 = 0.366 — well below 1.0
    GameState s = game_init();
    s.total_time = 316.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.366));
    REQUIRE(count_event(s, "stasis_warning") == 0);
}

TEST_CASE("game_tick: stasis_drain = 0.367 at total_time=317, stasis_warning NOT fired",
          "[milestone][stasis_warning][build29]") {
    // 0.05 + 0.001*317 = 0.367 — still below threshold
    GameState s = game_init();
    s.total_time = 317.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.367));
    REQUIRE(count_event(s, "stasis_warning") == 0);
}

TEST_CASE("game_tick: stasis_drain = 0.999 at total_time=949, stasis_warning NOT fired",
          "[milestone][stasis_warning][build29]") {
    // 0.05 + 0.001*949 = 0.999 — just below 1.0
    GameState s = game_init();
    s.total_time = 949.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.999));
    REQUIRE(count_event(s, "stasis_warning") == 0);
}

TEST_CASE("game_tick: stasis_drain = 1.0 at total_time=950, stasis_warning IS fired",
          "[milestone][stasis_warning][build29]") {
    // 0.05 + 0.001*950 = 1.0 — at threshold
    GameState s = game_init();
    s.total_time = 950.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(1.0));
    REQUIRE(count_event(s, "stasis_warning") == 1);
}

TEST_CASE("game_tick: stasis_warning milestone triggered flag set to true at t=950",
          "[milestone][stasis_warning][build29]") {
    GameState s = game_init();
    s.total_time = 950.0;
    game_tick(s, 0.0);
    bool triggered = false;
    for (const auto& m : s.milestones) {
        if (m.id == "stasis_warning") triggered = m.triggered;
    }
    REQUIRE(triggered == true);
}

// ===========================================================================
// Section 6 — Scrap boost step ordering: step 3 (decay) before step 4 (rate)
//
// Spec tick order:
//   Step 3: scrap_boost = max(0.0, scrap_boost - SCRAP_BOOST_DECAY * delta)
//   Step 4: tech_scrap.rate = robots * (BASE_SCRAP_RATE + compute * COMPUTE_SCRAP_BONUS)
//                           * (1.0 + scrap_boost)   ← uses post-decay boost
//
// With scrap_boost=0.1, delta=1.0, robots=1, compute=0:
//   Post-decay boost = 0.1 - 0.002 = 0.098
//   Correct rate  = 1 * 0.1 * (1 + 0.098) = 0.1098
//   Wrong rate    = 1 * 0.1 * (1 + 0.100) = 0.1100  (if step 4 ran first)
// ===========================================================================

TEST_CASE("game_tick: scrap rate uses post-decay boost (step 3 before step 4)",
          "[tick][scrap_boost][ordering][build29]") {
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 0.0;
    s.scrap_boost = 0.1;
    game_tick(s, 1.0);
    // Post-decay: 0.1 - 0.002*1.0 = 0.098
    REQUIRE(s.scrap_boost == Catch::Approx(0.098));
    // Rate uses decayed boost: 1 * 0.1 * (1 + 0.098) = 0.1098, NOT 0.11
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.1098));
}

// ===========================================================================
// Section 7 — Full scrap boost decay: 0.03 → 0.0 in exactly 15 ticks
//
// SCRAP_BOOST_BASE = 0.03; SCRAP_BOOST_DECAY = 0.002/s
// 0.03 / 0.002 = 15 seconds to reach 0.0 from a fresh single-click boost.
// After 14 ticks of delta=1.0: 0.03 - 14*0.002 = 0.002 (still positive)
// After 15 ticks:              0.002 - 0.002    = 0.000 (fully decayed)
// ===========================================================================

TEST_CASE("game_tick: scrap_boost=0.06 is still positive after 29 ticks of delta=1.0",
          "[tick][scrap_boost][decay][build29]") {
    // SCRAP_BOOST_BASE=0.06; 0.06 - 29*0.002 = 0.002 (still positive)
    GameState s = game_init();
    s.scrap_boost = 0.06;
    for (int i = 0; i < 29; ++i) game_tick(s, 1.0);
    REQUIRE(s.scrap_boost > 0.0);
}

TEST_CASE("game_tick: scrap_boost=0.06 decays to exactly 0.0 after 30 ticks of delta=1.0",
          "[tick][scrap_boost][decay][build29]") {
    // 0.06 - 30*0.002 = 0.0; max(0.0, 0.0) = 0.0
    GameState s = game_init();
    s.scrap_boost = 0.06;
    for (int i = 0; i < 30; ++i) game_tick(s, 1.0);
    REQUIRE(s.scrap_boost == Catch::Approx(0.0));
}

// ===========================================================================
// Section 8 — Milestone idempotency via game_tick across multiple ticks
//
// Spec: "Milestones are idempotent — triggered flag prevents re-firing."
// These tests verify the property holds through game_tick, not just
// check_milestones() in isolation.
// ===========================================================================

TEST_CASE("game_tick: first_compute milestone fires exactly once across two ticks",
          "[milestone][idempotency][build29]") {
    // No robots → compute is not reset by step 6; stays at 5.0 across ticks
    // first tick: fires (count=1); second tick: already triggered (count stays 1)
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "first_compute") == 1);
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "first_compute") == 1);  // still only once
}

TEST_CASE("game_tick: scrap_flowing milestone fires exactly once across two ticks",
          "[milestone][idempotency][build29]") {
    // No robots → tech_scrap.rate=0; value stays at 20.0 across ticks
    // first tick: fires; second tick: triggered flag prevents re-fire
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 20.0;
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "scrap_flowing") == 1);
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "scrap_flowing") == 1);  // still only once
}

// ===========================================================================
// Section 9 — Integration: wake_first_robot + buy_robot chain 1→5
//
// Spec costs:
//   wake_first_robot : 10 compute (robot 1)
//   buy_robot (2nd)  : 10 tech_scrap  (cost = 10 + 2*(1-1) = 10)
//   buy_robot (3rd)  : 12 tech_scrap  (cost = 10 + 2*(2-1) = 12)
//   buy_robot (4th)  : 14 tech_scrap  (cost = 10 + 2*(3-1) = 14)
//   buy_robot (5th)  : 16 tech_scrap  (cost = 10 + 2*(4-1) = 16)
//   Total scrap for robots 2-5 = 10 + 12 + 14 + 16 = 52
// ===========================================================================

TEST_CASE("integration: wake_first_robot + buy_robot chain 1→5 consumes exactly 52 tech_scrap",
          "[integration][robot][build29]") {
    GameState s = game_init();
    s.resources.at("compute").value    = 10.0; // required for wake
    s.resources.at("tech_scrap").value = 52.0; // exact cost for robots 2-5

    // Wake robot 1 (costs 10 compute)
    REQUIRE(wake_first_robot(s) == true);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(1.0));
    REQUIRE(s.first_robot_awoken == true);

    // Buy robots 2-5 with exactly 52 scrap
    REQUIRE(buy_robot(s) == true);  // 1→2, costs 10, scrap=42
    REQUIRE(buy_robot(s) == true);  // 2→3, costs 12, scrap=30
    REQUIRE(buy_robot(s) == true);  // 3→4, costs 14, scrap=16
    REQUIRE(buy_robot(s) == true);  // 4→5, costs 16, scrap=0

    REQUIRE(s.resources.at("robots").value     == Catch::Approx(5.0));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.0));
}

TEST_CASE("integration: buy_robot fails on 6th call (robots at cap 5.0)",
          "[integration][robot][build29]") {
    GameState s = game_init();
    s.resources.at("compute").value    = 10.0;
    s.resources.at("tech_scrap").value = 100.0;
    wake_first_robot(s);
    buy_robot(s); buy_robot(s); buy_robot(s); buy_robot(s); // 1→5
    // Now at cap
    REQUIRE(s.resources.at("robots").value == Catch::Approx(5.0));
    bool sixth = buy_robot(s);
    REQUIRE(sixth == false);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(5.0)); // unchanged
}

// ===========================================================================
// Section 10 — stasis_pod_count=25 drain at t=500
//
// Spec: cap = stasis_pod_count * STASIS_DRAIN_PER_POD = 25 * 0.4 = 10.0
// At t=500: formula = 0.05 + 0.003*500 = 1.55; min(1.55, 10.0) = 1.55 (not clamped)
// ===========================================================================

TEST_CASE("game_tick: stasis_pod_count=25, drain at total_time=500 is 0.55",
          "[stasis][pod_count][build29]") {
    // formula = 0.05 + 0.001*500 = 0.55; cap(25 pods)=25*2.0=50.0; STASIS_DRAIN_CAP=4.0
    // min(0.55, 50.0, 4.0) = 0.55 (below all caps)
    GameState s = game_init();
    s.stasis_pod_count = 25;
    s.total_time       = 500.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.55));
}

// ===========================================================================
// Section 11 — compute.cap=150 survives save/load after buy_compute_amplifier
//
// Spec: buy_compute_amplifier sets compute.cap=COMPUTE_AMP_CAP (150.0).
// The cap must be serialised and restored by save_game/load_game so the
// amplified buffer is maintained across sessions.
// ===========================================================================

TEST_CASE("round-trip: compute.cap=150 set by buy_compute_amplifier survives save/load",
          "[save_load][compute_amplifier][build29]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(150.0)); // precondition

    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("compute").cap == Catch::Approx(150.0));
    REQUIRE(loaded.compute_amplified == true);
}

TEST_CASE("round-trip: after restore, player_click respects amplified cap of 150 not 50",
          "[save_load][compute_amplifier][build29]") {
    // After buy + round-trip, compute.cap should be 150 so a click from 149 adds 1.0
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    GameState loaded = load_game(save_game(s));

    loaded.resources.at("compute").value = 149.0;
    loaded.resources.at("power").value   = 10.0;
    double added = player_click(loaded);
    REQUIRE(added == Catch::Approx(1.0));
    REQUIRE(loaded.resources.at("compute").value == Catch::Approx(150.0));
}

// ===========================================================================
// Section 12 — stasis_warning + colony_started fire in same tick; idempotent
//
// Both milestones can be satisfied simultaneously:
//   stasis_warning  : stasis_drain VALUE >= 1.0  (met at t=950, drain=1.0)
//   colony_started  : colony_capacity VALUE >= 2.0  (set directly)
//
// Both must appear in pending_events after the tick, and neither should
// appear a second time on a subsequent tick.
// ===========================================================================

TEST_CASE("game_tick: stasis_warning and colony_started both fire in same tick",
          "[milestone][multi][build29]") {
    // total_time=950 → drain=1.0 (stasis_warning threshold=1.0)
    // colony_capacity=2.0 (colony_started threshold=2.0)
    GameState s = game_init();
    s.total_time = 950.0;
    s.resources.at("colony_capacity").value = 2.0;
    game_tick(s, 0.0);
    REQUIRE(count_event(s, "stasis_warning")  == 1);
    REQUIRE(count_event(s, "colony_started")  == 1);
}

TEST_CASE("game_tick: stasis_warning and colony_started are idempotent across ticks",
          "[milestone][multi][idempotency][build29]") {
    // After both fire once in tick 1, tick 2 must not re-fire either
    GameState s = game_init();
    s.total_time = 950.0;
    s.resources.at("colony_capacity").value = 2.0;
    game_tick(s, 0.0);  // both fire here
    game_tick(s, 0.0);  // second tick — must not add duplicates
    REQUIRE(count_event(s, "stasis_warning") == 1);  // still only once total
    REQUIRE(count_event(s, "colony_started") == 1);
}
