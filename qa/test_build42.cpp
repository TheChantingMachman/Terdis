// Build 42 — test_build42.cpp
// New tests for build 42 spec changes:
//   1. SCRAP_BOOST_BASE raised from 0.03 to 0.06
//   2. Phase 1→2 gate colony_capacity threshold lowered from 10.0 to 2.0
//   3. colony_started milestone condition_value lowered from 10.0 to 2.0
//
// All values derived from spec/core-spec.md only.
//
// Constants:
//   SCRAP_BOOST_BASE         = 0.06
//   SCRAP_BOOST_COMPUTE_COST = 5.0
//   COLONY_CAPACITY_PER_UNIT = 2
//   COLONY_UNIT_BASE_COST    = 200.0
//
// Derived values (scrap_boost_click_delta = SCRAP_BOOST_BASE / (1.0 + boost / SCRAP_BOOST_BASE)):
//   At 0.00: 0.06 / 1.0 = 0.06
//   At 0.06: 0.06 / 2.0 = 0.03
//   At 0.12: 0.06 / 3.0 = 0.02

#include <catch2/catch_all.hpp>
#include <algorithm>
#include "custodian.hpp"

// ===========================================================================
// Section 1 — SCRAP_BOOST_BASE = 0.06: additional delta values
// ===========================================================================

TEST_CASE("scrap_boost_click_delta at scrap_boost=0.12 returns 0.02", "[scrap_boost][delta][build42]") {
    // 0.06 / (1.0 + 0.12 / 0.06) = 0.06 / 3.0 = 0.02
    GameState s = game_init();
    s.scrap_boost = 0.12;
    REQUIRE(scrap_boost_click_delta(s) == Catch::Approx(0.02));
}

TEST_CASE("scrap_boost_click_delta diminishes with increasing boost (new base=0.06)", "[scrap_boost][delta][build42]") {
    // Verify the diminishing-return property holds across three increasing boost values
    GameState s = game_init();
    s.scrap_boost = 0.0;
    double d0 = scrap_boost_click_delta(s);
    s.scrap_boost = 0.06;
    double d1 = scrap_boost_click_delta(s);
    s.scrap_boost = 0.12;
    double d2 = scrap_boost_click_delta(s);
    REQUIRE(d0 > d1);
    REQUIRE(d1 > d2);
}

TEST_CASE("apply_scrap_boost three calls sequence matches new base=0.06 formula", "[scrap_boost][success][build42]") {
    // Call 1: boost=0.0,  delta=0.06 → boost=0.06, compute=15→10
    // Call 2: boost=0.06, delta=0.03 → boost=0.09, compute=10→5
    // Call 3: boost=0.09, delta=0.06/(1+0.09/0.06)=0.06/2.5=0.024 → boost=0.114, compute=5→0
    GameState s = game_init();
    s.resources.at("compute").value = 15.0;
    s.scrap_boost = 0.0;

    apply_scrap_boost(s); // boost → 0.06, compute → 10.0
    REQUIRE(s.scrap_boost == Catch::Approx(0.06));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(10.0));

    apply_scrap_boost(s); // boost → 0.09, compute → 5.0
    REQUIRE(s.scrap_boost == Catch::Approx(0.09));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(5.0));

    apply_scrap_boost(s); // boost → 0.114, compute → 0.0
    REQUIRE(s.scrap_boost == Catch::Approx(0.114));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

// ===========================================================================
// Section 2 — Phase 1→2 gate: colony_capacity threshold = 2.0
// ===========================================================================

TEST_CASE("check_phase_gates: colony_capacity=2.0 and power.rate=0.0 advances to phase 2",
          "[gates][build42]") {
    // Exact new threshold: colony_capacity VALUE >= 2.0, power RATE >= 0.0
    GameState s = game_init();
    s.resources.at("power").rate            = 0.0;
    s.resources.at("colony_capacity").value = 2.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates: colony_capacity=1.9 (just below 2.0) does not advance",
          "[gates][build42]") {
    // 1.9 < 2.0 → gate must fail even with positive power.rate
    GameState s = game_init();
    s.resources.at("power").rate            = 1.0;
    s.resources.at("colony_capacity").value = 1.9;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates: colony_capacity=0.0 (default) does not advance even with power.rate>0",
          "[gates][build42]") {
    // Default colony_capacity=0.0 < 2.0 → gate always fails at game start
    GameState s = game_init();
    s.resources.at("power").rate = 5.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

// ===========================================================================
// Section 3 — colony_started milestone: condition_value = 2.0
// ===========================================================================

TEST_CASE("check_milestones: colony_started triggers at exactly 2.0", "[milestones][colony][build42]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 2.0;
    check_milestones(s);
    bool triggered = false;
    for (const auto& m : s.milestones) {
        if (m.id == "colony_started") triggered = m.triggered;
    }
    REQUIRE(triggered == true);
}

TEST_CASE("check_milestones: colony_started does not trigger at 1.9 (below 2.0)",
          "[milestones][colony][build42]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 1.9;
    check_milestones(s);
    bool triggered = false;
    for (const auto& m : s.milestones) {
        if (m.id == "colony_started") triggered = m.triggered;
    }
    REQUIRE(triggered == false);
}

TEST_CASE("check_milestones: colony_started event fired at exactly 2.0",
          "[milestones][colony][build42]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 2.0;
    check_milestones(s);
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "colony_started")
                 != s.pending_events.end();
    REQUIRE(found);
}

// ===========================================================================
// Section 4 — Integration: single colony unit purchase triggers phase advance
//
// buy_colony_unit (count=0): costs 200 scrap, adds COLONY_CAPACITY_PER_UNIT=2
// After purchase: colony_capacity=2.0 >= 2.0
// With positive power.rate: phase advances to 2 immediately via check_phase_gates
// ===========================================================================

TEST_CASE("integration: single colony unit purchase satisfies phase 1→2 gate",
          "[integration][colony][gates][build42]") {
    // One colony unit sets colony_capacity=2.0, which meets the new gate threshold.
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 200.0; // exact cost for first unit
    s.resources.at("power").rate       = 0.1;   // positive power.rate meets gate condition

    bool bought = buy_colony_unit(s);
    REQUIRE(bought == true);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(2.0));

    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("integration: single colony unit purchase fires colony_started milestone",
          "[integration][colony][milestones][build42]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 200.0;

    buy_colony_unit(s);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(2.0));

    check_milestones(s);
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "colony_started")
                 != s.pending_events.end();
    REQUIRE(found);
}

TEST_CASE("integration: game_tick with colony_capacity=2.0 and positive power.rate advances phase",
          "[integration][colony][tick][build42]") {
    // game_tick calls check_phase_gates internally — verify end-to-end via tick
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 2.0;
    // Force positive power.rate by pre-setting it (no robots, no solar panels beyond base)
    // Base power.rate = 0.1 (positive) at t=0 with no stasis pods draining more than base
    s.stasis_pod_count = 0; // zero drain so power.rate = BASE_POWER_RATE = 0.1
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("integration: phase does not advance if colony_capacity < 2.0 after failed buy",
          "[integration][colony][gates][build42]") {
    // Not enough scrap for colony unit → colony_capacity stays 0.0 → gate fails
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 100.0; // below 200.0 cost
    s.resources.at("power").rate       = 5.0;   // power condition met

    buy_colony_unit(s); // should fail
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(0.0));

    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}
