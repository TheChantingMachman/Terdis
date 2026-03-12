// Build 47 — test_build47.cpp
// Tests for updated game-balance constants:
//   C1. SOLAR_DECAY_RATE 0.005 → 0.002
//   C2. HUMAN_POWER_BASE 1.0  → 0.5
//   C3. HUMAN_POWER_SCALE 0.5 → 0.3
//
// Formula: solar_decay_accumulator += SOLAR_DECAY_RATE * (solar_panel_count / SOLAR_PANEL_MAX) * delta
// Formula: human_power_cost(n) = n * HUMAN_POWER_BASE + HUMAN_POWER_SCALE * n*(n-1)/2
//          = n*0.5 + 0.3*n*(n-1)/2
//
// Human power cost reference (new values):
//   n=1:  0.5    n=2:  1.3    n=3:  2.4    n=5:  5.5    n=10: 18.5
//
// Solar decay reference (new values, 20 panels = SOLAR_PANEL_MAX):
//   Rate at max density:  0.002 * (20/20) = 0.002/s  → threshold at delta=500
//   Rate at half density: 0.002 * (10/20) = 0.001/s  → threshold at delta=1000
//
// All values derived from spec/core-spec.md.

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ===========================================================================
// Section 1 — SOLAR_DECAY_RATE = 0.002 (normalized formula)
// Verify the rate through accumulator and degradation behaviour.
// ===========================================================================

TEST_CASE("build47 C1: accumulator at max density after 500s equals exactly 1.0 (threshold)", "[solar][decay][build47]") {
    // 0.002 * (20/20) * 500 = 1.0 → accumulator reaches threshold exactly once
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 500.0);
    REQUIRE(s.solar_panel_count == 19); // one panel lost
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0)); // wraps to 0
}

TEST_CASE("build47 C1: accumulator at max density after 499s is below 1.0 (no degradation)", "[solar][decay][build47]") {
    // 0.002 * (20/20) * 499 = 0.998 < 1.0 → no degradation
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 499.0);
    REQUIRE(s.solar_panel_count == 20);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.002 * (20.0 / 20.0) * 499.0));
}

TEST_CASE("build47 C1: accumulator at max density after 1000s triggers two degradations", "[solar][decay][build47]") {
    // 0.002 * (20/20) * 1000 = 2.0 → fires twice
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 1000.0);
    REQUIRE(s.solar_panel_count == 18);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("build47 C1: accumulator at half density after 1000s equals exactly 1.0 (threshold)", "[solar][decay][build47]") {
    // 0.002 * (10/20) * 1000 = 1.0 → one degradation at half density
    GameState s = game_init();
    s.solar_panel_count = 10;
    s.resources.at("power").value = 100.0;
    game_tick(s, 1000.0);
    REQUIRE(s.solar_panel_count == 9);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("build47 C1: accumulator at half density after 999s is below 1.0 (no degradation)", "[solar][decay][build47]") {
    // 0.002 * (10/20) * 999 = 0.999 < 1.0
    GameState s = game_init();
    s.solar_panel_count = 10;
    s.resources.at("power").value = 100.0;
    game_tick(s, 999.0);
    REQUIRE(s.solar_panel_count == 10);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.002 * (10.0 / 20.0) * 999.0));
}

TEST_CASE("build47 C1: decay rate is proportional to panel count (5 panels, delta=2000)", "[solar][decay][build47]") {
    // 0.002 * (5/20) * 2000 = 1.0 → exactly one degradation at quarter density
    GameState s = game_init();
    s.solar_panel_count = 5;
    s.resources.at("power").value = 100.0;
    game_tick(s, 2000.0);
    REQUIRE(s.solar_panel_count == 4);
    REQUIRE(s.solar_decay_accumulator == Catch::Approx(0.0));
}

TEST_CASE("build47 C1: old unnormalized threshold (delta=25) causes no degradation with normalized formula", "[solar][decay][build47]") {
    // Old (unnormalized): 0.002 * 20 * 25 = 1.0 → 1 panel lost
    // New (normalized):   0.002 * (20/20) * 25 = 0.05 < 1.0 → no degradation
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 25.0);
    REQUIRE(s.solar_panel_count == 20); // normalized formula: no degradation at 25s
}

// ===========================================================================
// Section 2 — HUMAN_POWER_BASE = 0.5, HUMAN_POWER_SCALE = 0.3
// Verify new cost values through recompute_rates power.rate output.
// Baseline: power.rate = 0.1 - human_cost(n) - 0.05  (0 robots, 0 panels)
// ===========================================================================

TEST_CASE("build47 C2+C3: human_power_cost n=1 → 0.5, power.rate = -0.45", "[rates][humans][build47]") {
    // cost(1) = 1*0.5 + 0.3*1*0/2 = 0.5
    // power.rate = 0.1 - 0.5 - 0.05 = -0.45
    GameState s = game_init();
    s.awoken_humans = 1;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

TEST_CASE("build47 C2+C3: human_power_cost n=2 → 1.3, power.rate = -1.25", "[rates][humans][build47]") {
    // cost(2) = 2*0.5 + 0.3*2*1/2 = 1.0 + 0.3 = 1.3
    // power.rate = 0.1 - 1.3 - 0.05 = -1.25
    GameState s = game_init();
    s.awoken_humans = 2;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-1.25));
}

TEST_CASE("build47 C2+C3: human_power_cost n=3 → 2.4, power.rate = -2.35", "[rates][humans][build47]") {
    // cost(3) = 3*0.5 + 0.3*3*2/2 = 1.5 + 0.9 = 2.4
    // power.rate = 0.1 - 2.4 - 0.05 = -2.35
    GameState s = game_init();
    s.awoken_humans = 3;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-2.35));
}

TEST_CASE("build47 C2+C3: human_power_cost n=5 → 5.5, power.rate = -5.45", "[rates][humans][build47]") {
    // cost(5) = 5*0.5 + 0.3*5*4/2 = 2.5 + 3.0 = 5.5
    // power.rate = 0.1 - 5.5 - 0.05 = -5.45
    GameState s = game_init();
    s.awoken_humans = 5;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-5.45));
}

TEST_CASE("build47 C2+C3: human_power_cost n=10 → 18.5, power.rate = -18.45", "[rates][humans][build47]") {
    // cost(10) = 10*0.5 + 0.3*10*9/2 = 5.0 + 13.5 = 18.5
    // power.rate = 0.1 - 18.5 - 0.05 = -18.45
    GameState s = game_init();
    s.awoken_humans = 10;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-18.45));
}

TEST_CASE("build47 C2+C3: scaling is superlinear — cost(5) < 5 * cost(1)", "[rates][humans][build47]") {
    // cost(5)=5.5 vs 5*cost(1)=5*0.5=2.5: 5.5 > 2.5 → cost is superlinear in n
    // Verify: n=5 rate is more negative than n=1 rate * 5
    // n=1 rate = -0.45; 5 * -0.45 = -2.25
    // n=5 rate = -5.45 < -2.25 → verified
    GameState s1 = game_init();
    s1.awoken_humans = 1;
    recompute_rates(s1);
    double rate1 = s1.resources.at("power").rate; // -0.45

    GameState s5 = game_init();
    s5.awoken_humans = 5;
    recompute_rates(s5);
    double rate5 = s5.resources.at("power").rate; // -5.45

    REQUIRE(rate5 < 5.0 * rate1); // more negative than linear scaling
}

TEST_CASE("build47 C2+C3: awaken_human recompute reflects new cost (n=1)", "[awaken_human][build47]") {
    // After awakening 1 human, power.rate must use new cost(1)=0.5
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("colony_capacity").value = 10.0;
    s.stasis_pod_count = 10;
    s.awoken_humans = 0;
    s.resources.at("power").rate = 999.0; // sentinel
    awaken_human(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

TEST_CASE("build47 C2+C3: game_tick recompute reflects new human cost (n=2, delta=0)", "[tick][humans][build47]") {
    // game_tick with delta=0 runs recompute_rates; verify n=2 cost applies
    GameState s = game_init();
    s.awoken_humans = 2;
    s.resources.at("power").rate = 999.0; // sentinel
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-1.25));
}

// ===========================================================================
// Section 3 — Cross-check: old constant values no longer produce expected results
// ===========================================================================

TEST_CASE("build47 C1: old threshold delta=200 causes no degradation with normalized formula", "[solar][decay][build47]") {
    // Old unnormalized (0.002): 0.002 * 20 * 200 = 8.0 → 8 degradations
    // New normalized (0.002):   0.002 * (20/20) * 200 = 0.4 < 1.0 → 0 degradations
    GameState s = game_init();
    s.solar_panel_count = 20;
    s.resources.at("power").value = 100.0;
    game_tick(s, 200.0);
    REQUIRE(s.solar_panel_count == 20); // normalized: no degradation at 200s
}

TEST_CASE("build47 C2+C3: n=1 cost is no longer 1.0 (old HUMAN_POWER_BASE)", "[rates][humans][build47]") {
    // Old: cost(1)=1.0 → power.rate=-0.95
    // New: cost(1)=0.5 → power.rate=-0.45  (NOT -0.95)
    GameState s = game_init();
    s.awoken_humans = 1;
    recompute_rates(s);
    REQUIRE_FALSE(s.resources.at("power").rate == Catch::Approx(-0.95));
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.45));
}

TEST_CASE("build47 C2+C3: n=5 cost is no longer 10.0 (old formula)", "[rates][humans][build47]") {
    // Old: 5*1.0 + 0.5*5*4/2 = 5+5 = 10.0 → rate=-9.95
    // New: 5*0.5 + 0.3*5*4/2 = 2.5+3.0 = 5.5 → rate=-5.45
    GameState s = game_init();
    s.awoken_humans = 5;
    recompute_rates(s);
    REQUIRE_FALSE(s.resources.at("power").rate == Catch::Approx(-9.95));
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-5.45));
}
