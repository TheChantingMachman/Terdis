// Build 25 — test_phase1_compliance_build25.cpp
// Final Phase 1 compliance tests covering gaps identified across the existing qa/ suite.
// All values derived from spec/core-spec.md.
//
// Covers:
//   Section 1: recompute_rates with elevated stasis_drain.value (dynamic, not fixed 0.05)
//   Section 2: player_click edge cases (power still deducted when compute at cap; power=0.0)
//   Section 3: game_tick stasis_drain.rate and colony_capacity.rate remain 0 always
//   Section 4: dynamic scrap rate with 2+ robots and non-zero compute
//   Section 5: game_tick ordering — stasis update (step 1) precedes recompute_rates (step 2)
//   Section 6: save/load round-trip for first_robot_awoken
//   Section 7: integration — full Phase 1 → Phase 2 via purchases + game_tick
//   Section 8: integration — compute amplifier unlocks higher compute → elevated scrap rate

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ===========================================================================
// Section 1 — recompute_rates with elevated stasis_drain.value
//
// Spec formula:
//   power.rate = BASE_POWER_RATE + solar_panel_count * SOLAR_POWER_RATE
//              - robots.value * ROBOT_POWER_DRAIN
//              - resources["stasis_drain"].value
//
// Existing tests only call recompute_rates with stasis_drain.value=0.05 (initial).
// These tests verify the live map value is used, not a hardcoded constant.
// ===========================================================================

TEST_CASE("recompute_rates: stasis_drain=0.5, no panels, no robots → power.rate=-0.4",
          "[rates][stasis][build25]") {
    // 0.1 + 0*0.5 - 0*0.2 - 0.5 = -0.4
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 0.5;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.4));
}

TEST_CASE("recompute_rates: stasis_drain=1.0, 3 panels, 2 robots → power.rate=0.95",
          "[rates][stasis][build25]") {
    // 0.1 + 3*0.75 - 2*0.2 - 1.0 = 0.1 + 2.25 - 0.4 - 1.0 = 0.95
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 1.0;
    s.solar_panel_count = 3;
    s.resources.at("robots").value = 2.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.95));
}

TEST_CASE("recompute_rates: stasis_drain=2.0, 5 panels, 5 robots → power.rate=0.85",
          "[rates][stasis][build25]") {
    // 0.1 + 5*0.75 - 5*0.2 - 2.0 = 0.1 + 3.75 - 1.0 - 2.0 = 0.85
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 2.0;
    s.solar_panel_count = 5;
    s.resources.at("robots").value = 5.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.85));
}

TEST_CASE("recompute_rates: stasis_drain=0.8, 2 panels, 1 robot → power.rate=0.60",
          "[rates][stasis][build25]") {
    // 0.1 + 2*0.75 - 1*0.2 - 0.8 = 0.1 + 1.5 - 0.2 - 0.8 = 0.60
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 0.8;
    s.solar_panel_count = 2;
    s.resources.at("robots").value = 1.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.60));
}

// ===========================================================================
// Section 2 — player_click edge cases
//
// Spec: "Costs click_cost power. If power.value < click_cost, returns 0.0 and
//        does nothing. Adds click_power compute to compute.value (clamped to cap).
//        Returns the amount of compute actually added."
//
// When power is sufficient but compute is at cap:
//   - power cost is still paid (only insufficient power aborts the click)
//   - compute stays at cap; 0.0 compute added; returns 0.0
// ===========================================================================

TEST_CASE("player_click: power is still deducted when compute is already at cap",
          "[click][edge][build25]") {
    // compute=50.0 (cap), power=15.0 → click deducts 1.0 power, adds 0 compute, returns 0.0
    GameState s = game_init();
    s.resources.at("compute").value = 50.0; // at cap
    s.resources.at("power").value   = 15.0;
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(0.0));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(50.0)); // stays at cap
    REQUIRE(s.resources.at("power").value   == Catch::Approx(14.0)); // cost paid
}

TEST_CASE("player_click: power=0.0 exactly is insufficient — returns 0.0, nothing changes",
          "[click][edge][build25]") {
    // 0.0 < click_cost(1.0) → abort; no state change
    GameState s = game_init();
    s.resources.at("power").value   = 0.0;
    s.resources.at("compute").value = 5.0;
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(0.0));
    REQUIRE(s.resources.at("power").value   == Catch::Approx(0.0));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(5.0));
}

// ===========================================================================
// Section 3 — game_tick: stasis_drain.rate and colony_capacity.rate invariants
//
// Spec: "stasis_drain and colony_capacity have rate=0 always. Their values are set
//        directly — stasis_drain by the tick engine each frame, colony_capacity by
//        buy_colony_unit."
// recompute_rates sets both rates to 0.0 (step 2), and game_tick must not alter them.
// ===========================================================================

TEST_CASE("game_tick: stasis_drain.rate remains 0.0 after tick",
          "[tick][stasis][build25]") {
    GameState s = game_init();
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("stasis_drain").rate == Catch::Approx(0.0));
}

TEST_CASE("game_tick: colony_capacity.rate remains 0.0 after tick",
          "[tick][colony][build25]") {
    GameState s = game_init();
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("colony_capacity").rate == Catch::Approx(0.0));
}

TEST_CASE("game_tick: stasis_drain.value changes but stasis_drain.rate stays 0",
          "[tick][stasis][build25]") {
    // At t=100: stasis_drain.value = 0.05 + 0.001*100 = 0.15 (value set by tick step 1)
    // stasis_drain.rate must remain 0.0 — value changes are direct assignment, not rate-driven
    GameState s = game_init();
    s.total_time = 100.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.15));
    REQUIRE(s.resources.at("stasis_drain").rate  == Catch::Approx(0.0));
}

TEST_CASE("game_tick: colony_capacity.value is preserved through tick (rate=0 invariant)",
          "[tick][colony][build25]") {
    // colony_capacity.rate=0 always → tick step 4 adds 0*delta → value unchanged
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 30.0; // set as if buy_colony_unit called 3×
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(30.0));
}

// ===========================================================================
// Section 4 — dynamic scrap rate with multiple robots and non-zero compute
//
// Spec formula: tech_scrap.rate = robots.value * (BASE_SCRAP_RATE + compute.value * COMPUTE_SCRAP_BONUS)
//               BASE_SCRAP_RATE=0.1, COMPUTE_SCRAP_BONUS=0.02
//
// Existing tests cover: 1 robot with various compute values.
// These tests add 2-robot and 5-robot scenarios with compute > 0.
// ===========================================================================

TEST_CASE("game_tick: scrap rate with 2 robots and 5 compute → 0.4/s",
          "[tick][scrap][build25]") {
    // rate = 2 * (0.1 + 5*0.02) = 2 * 0.2 = 0.4; value after 1s = 0.4
    GameState s = game_init();
    s.resources.at("robots").value  = 2.0;
    s.resources.at("compute").value = 5.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate  == Catch::Approx(0.4));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.4));
}

TEST_CASE("game_tick: scrap rate with 5 robots (full workforce) and 0 compute → 0.5/s",
          "[tick][scrap][build25]") {
    // rate = 5 * (0.1 + 0*0.02) = 0.5; value after 1s = 0.5
    GameState s = game_init();
    s.resources.at("robots").value  = 5.0;
    s.resources.at("compute").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate  == Catch::Approx(0.5));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.5));
}

TEST_CASE("game_tick: scrap rate with 3 robots and 10 compute → 0.9/s",
          "[tick][scrap][build25]") {
    // rate = 3 * (0.1 + 10*0.02) = 3 * 0.3 = 0.9; value after 1s = 0.9
    GameState s = game_init();
    s.resources.at("robots").value  = 3.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate  == Catch::Approx(0.9));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.9));
}

// ===========================================================================
// Section 5 — game_tick ordering verification
//
// Tick steps (from spec):
//   1. Update stasis_drain.value using pre-tick total_time
//   2. recompute_rates (reads the updated stasis_drain.value; sets compute.rate)
//   3. Decay boost
//   4. Set dynamic scrap rate (reads compute.value before advance)
//   5. Advance resources (drains compute via compute.rate)
//   6. Power deficit timer
// ===========================================================================

TEST_CASE("game_tick ordering: stasis update (step 1) feeds into recompute_rates (step 2)",
          "[tick][ordering][build25]") {
    // total_time=200 (pre-tick):
    //   step 1: stasis_drain.value = min(0.05 + 0.001*200, 4.0) = 0.25
    //   step 2: power.rate = 0.1 + 0*0.75 - 0*0.2 - 0.25 = -0.15
    // If ordering were wrong (recompute before stasis update), power.rate would use
    // the old stasis_drain.value (0.05), giving +0.05 instead of -0.15 → bug caught.
    GameState s = game_init();
    s.total_time = 200.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.25));
    REQUIRE(s.resources.at("power").rate          == Catch::Approx(-0.15));
}

TEST_CASE("game_tick ordering: scrap rate set (step 4) uses pre-advance compute",
          "[tick][ordering][build25]") {
    // robots=1, compute=10:
    //   step 4: tech_scrap.rate = 1*(0.1 + 10*0.02) = 0.3   (reads compute=10 before advance)
    //   step 5: tech_scrap.value advances by 0.3*1s = 0.3; compute.rate=-0.5 → clamp(10-0.5, 0, 50) = 9.5
    // Verifies scrap rate and value both reflect pre-advance compute value
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate  == Catch::Approx(0.3));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.3));
    REQUIRE(s.resources.at("compute").value    == Catch::Approx(9.5)); // drained by advance step
}

// ===========================================================================
// Section 6 — save/load: first_robot_awoken round-trip
//
// first_robot_awoken is a critical boolean gate that prevents wake_first_robot
// from being called twice and guards buy_robot. It must survive save/load.
// ===========================================================================

TEST_CASE("round-trip: first_robot_awoken=false is preserved",
          "[save_load][robot][build25]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.first_robot_awoken == false);
}

TEST_CASE("round-trip: first_robot_awoken=true is preserved",
          "[save_load][robot][build25]") {
    GameState s = game_init();
    s.first_robot_awoken = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.first_robot_awoken == true);
}

// ===========================================================================
// Section 7 — integration: full Phase 1 → Phase 2 path
//
// Phase 1→2 gate (from spec):
//   power RATE >= 0.0   (self-sufficient)
//   colony_capacity VALUE >= 10.0   (first colony unit built)
// ===========================================================================

TEST_CASE("integration: buy_solar_panel + 5×buy_colony_unit → game_tick fires Phase 1→2 gate",
          "[integration][phase][build25]") {
    // COLONY_CAPACITY_PER_UNIT=2; need 5 units to reach colony_capacity=10.0 (gate requires ≥10.0)
    // Colony costs: 200 + 240 + 288 + 345.6 + 414.72 = 1488.32; solar=5.0; total≈1493.32
    // After 5 colony units: colony_capacity.value = 10.0
    // buy_solar_panel (cost 5.0): power.rate = 0.1 + 0.5 - 0.05 = 0.55 >= 0.0
    // game_tick: check_phase_gates → both conditions met → phase 2
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 1500.0;
    bool r1 = buy_colony_unit(s); // cost=200.0, capacity=2.0
    bool r2 = buy_colony_unit(s); // cost=240.0, capacity=4.0
    bool r3 = buy_colony_unit(s); // cost=288.0, capacity=6.0
    bool r4 = buy_colony_unit(s); // cost=345.6, capacity=8.0
    bool r5 = buy_colony_unit(s); // cost=414.72, capacity=10.0
    REQUIRE(r1 == true);
    REQUIRE(r5 == true);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(10.0));
    bool solar = buy_solar_panel(s); // cost=5.0
    REQUIRE(solar == true);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.80));
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("integration: without colony unit built, phase gate does not fire even with positive power rate",
          "[integration][phase][build25]") {
    // power.rate is already >= 0.0 at init (0.05), but colony_capacity=0 < 10 → no phase advance
    GameState s = game_init();
    // stasis_drain=0.05; power.rate = 0.1 - 0.05 = 0.05 >= 0.0 at t=0
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 1); // gate not met: colony_capacity.value=0 < 10
}

TEST_CASE("integration: scrap_flowing milestone fires via game_tick when tech_scrap reaches 20.0",
          "[integration][milestone][build25]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 20.0;
    game_tick(s, 0.0);
    bool found = false;
    for (const auto& ev : s.pending_events) {
        if (ev == "scrap_flowing") { found = true; break; }
    }
    REQUIRE(found);
}

TEST_CASE("integration: wake_first_robot + 2 solar panels triggers solar_online milestone via tick",
          "[integration][milestone][build25]") {
    // solar_online: power RATE >= 0.6
    // After wake (1 robot, 0 panels): power.rate = 0.1 - 0.2 - 0.05 = -0.15
    // After panel 1 (cost 5.0):  power.rate = 0.1 + 0.75 - 0.2 - 0.05 = 0.60
    // After panel 2 (cost 5.5):  power.rate = 0.1 + 1.50 - 0.2 - 0.05 = 1.35 >= 0.6 ✓
    GameState s = game_init();
    s.resources.at("compute").value    = 10.0;
    wake_first_robot(s);                       // robots=1, first_robot_awoken=true
    s.resources.at("tech_scrap").value = 20.0; // 5.0 + 5.5 = 10.5 needed; 20.0 is enough
    buy_solar_panel(s);                        // panel 1: cost 5.0
    buy_solar_panel(s);                        // panel 2: cost 5.5
    REQUIRE(s.resources.at("power").rate == Catch::Approx(1.35));
    game_tick(s, 0.0);
    bool found = false;
    for (const auto& ev : s.pending_events) {
        if (ev == "solar_online") { found = true; break; }
    }
    REQUIRE(found);
}

// ===========================================================================
// Section 8 — integration: compute amplifier unlocks higher compute → scrap rate
//
// Spec: buy_compute_amplifier sets compute.cap=150.0 (was 50.0)
// Gameplay impact: robot accumulates more compute between ticks →
//   tech_scrap.rate = robots * (0.1 + compute * 0.02) is much higher
// ===========================================================================

TEST_CASE("integration: compute amplifier allows compute above 50; robot uses it for scrap rate",
          "[integration][compute_amplifier][build25]") {
    // After amplifier: compute.cap=150; set compute=100 (impossible before amplifier)
    // 1 robot: tech_scrap.rate = 1*(0.1 + 100*0.02) = 1*(0.1 + 2.0) = 2.1
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);  // compute.cap → 150.0, scrap → 0.0
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(150.0));
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 100.0; // only reachable after amplifier
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(2.1));
    REQUIRE(s.resources.at("compute").value   == Catch::Approx(99.5)); // drained by 0.5: 100-0.5=99.5
}

TEST_CASE("integration: player_click accumulates up to amplified cap of 150.0",
          "[integration][compute_amplifier][build25]") {
    // After amplifier: compute.cap=150; clicking from 149 should add 1.0 (not capped at 50)
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    s.resources.at("compute").value = 149.0;
    s.resources.at("power").value   = 10.0;
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(1.0));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(150.0));
}
