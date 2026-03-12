// Build 32 — test_compliance_build32.cpp
// Tests for the compute-drain rework:
//   - ROBOT_COMPUTE_DRAIN = 0.5 (changed from 1.0)
//   - recompute_rates: compute.rate = -robots.value * ROBOT_COMPUTE_DRAIN
//   - game_tick: compute drains continuously via rate; old instant-zero step removed
// All values derived from spec/core-spec.md.

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ===========================================================================
// Section 1 — recompute_rates: compute.rate formula
//
// Formula: compute.rate = -robots.value * ROBOT_COMPUTE_DRAIN
//          ROBOT_COMPUTE_DRAIN = 0.5
// ===========================================================================

TEST_CASE("recompute_rates: 1 robot → compute.rate = -0.5", "[rates][compute][build32]") {
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-0.5));
}

TEST_CASE("recompute_rates: 3 robots → compute.rate = -1.5", "[rates][compute][build32]") {
    GameState s = game_init();
    s.resources.at("robots").value = 3.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-1.5));
}

TEST_CASE("recompute_rates: 5 robots → compute.rate = -2.5", "[rates][compute][build32]") {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-2.5));
}

TEST_CASE("recompute_rates: 0 robots → compute.rate = 0.0", "[rates][compute][build32]") {
    // ROBOT_COMPUTE_DRAIN formula: -0.0 * 1.0 = 0.0 — no drain without robots
    GameState s = game_init();
    // robots.value is 0.0 at init
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(0.0));
}

TEST_CASE("recompute_rates: compute.rate independent of solar panels", "[rates][compute][build32]") {
    // Solar panels affect power.rate only; compute.rate depends solely on robots
    // 2 robots * 0.5 = 1.0
    GameState s = game_init();
    s.solar_panel_count = 5;
    s.resources.at("robots").value = 2.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-1.0));
}

// ===========================================================================
// Section 2 — game_tick: continuous compute drain via advance step
//
// Formula: compute_after = clamp(compute_start + compute.rate * delta, 0.0, compute.cap)
//          With R robots: compute.rate = -R * 0.5
// ===========================================================================

TEST_CASE("game_tick: 1 robot drains compute from 10.0 to 9.5 over 1s", "[tick][compute][build32]") {
    // compute.rate = -0.5; clamp(10 + (-0.5)*1, 0, 50) = 9.5
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(9.5));
}

TEST_CASE("game_tick: 1 robot drains compute over fractional delta 0.5s", "[tick][compute][build32]") {
    // compute.rate = -0.5; clamp(10 + (-0.5)*0.5, 0, 50) = 9.75
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 0.5);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(9.75));
}

TEST_CASE("game_tick: 5 robots drain compute from 10.0 to 7.5 over 1s", "[tick][compute][build32]") {
    // compute.rate = -2.5; clamp(10 + (-2.5)*1, 0, 50) = 7.5
    GameState s = game_init();
    s.resources.at("robots").value  = 5.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(7.5));
}

TEST_CASE("game_tick: compute clamped at 0 when drain would go negative", "[tick][compute][build32]") {
    // 5 robots, compute=2.0, delta=1.0: 2 + (-2.5)*1 = -0.5 → clamped to 0.0
    GameState s = game_init();
    s.resources.at("robots").value  = 5.0;
    s.resources.at("compute").value = 2.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("game_tick: compute stays 0 when already 0 and robots active", "[tick][compute][build32]") {
    // No underflow: clamp(0 + (-1)*1, 0, 50) = 0.0
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("game_tick: no compute drain when robots=0 (rate=0)", "[tick][compute][build32]") {
    // compute.rate = 0.0; value unchanged regardless of delta
    GameState s = game_init();
    s.resources.at("compute").value = 20.0;
    // robots.value = 0 at init
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(20.0));
}

// ===========================================================================
// Section 3 — Verify old instant-zero behavior is gone
//
// Previously: if robots.value > 0, compute.value was set to 0.0 immediately
//             regardless of delta.
// Now: compute drains via rate * delta — so delta=0.0 means zero drain.
// ===========================================================================

TEST_CASE("game_tick: zero delta does not drain compute even with robots active", "[tick][compute][build32]") {
    // Old behavior would have zeroed compute; new behavior: delta=0 → no advance → no drain
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 8.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(8.0));
}

TEST_CASE("game_tick: small delta produces partial drain, not instant zero", "[tick][compute][build32]") {
    // With robots=1, compute=50, delta=0.1: clamp(50 + (-0.5)*0.1, 0, 50) = 49.95 (not 0)
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 50.0;
    game_tick(s, 0.1);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(49.95));
}

// ===========================================================================
// Section 4 — Multi-tick cumulative drain
//
// Each tick drains robots * ROBOT_COMPUTE_DRAIN * delta from compute.
// ===========================================================================

TEST_CASE("game_tick: 3 ticks of 1s each drain compute by 1.5 with 1 robot", "[tick][compute][build32]") {
    // Start: compute=10.0; ROBOT_COMPUTE_DRAIN=0.5; after 3 ticks of 1s: 10→9.5→9.0→8.5
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 1.0);
    game_tick(s, 1.0);
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(8.5));
}

TEST_CASE("game_tick: compute drains to 0 and stays at 0 over successive ticks", "[tick][compute][build32]") {
    // 1 robot, ROBOT_COMPUTE_DRAIN=0.5; compute=2.0
    // tick 1: 2-0.5=1.5; tick 2: 1.5-0.5=1.0; tick 3: 1.0-0.5=0.5; tick 4: 0.5-0.5=0.0; tick 5: stays 0
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 2.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(1.5));
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(1.0));
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.5));
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0)); // stays at 0
}

// ===========================================================================
// Section 5 — compute.rate is set by recompute_rates inside game_tick
//
// game_tick calls recompute_rates (step 2) which sets compute.rate.
// The rate is then readable after the tick.
// ===========================================================================

TEST_CASE("game_tick: compute.rate is -0.5 after tick with 1 active robot", "[tick][compute][build32]") {
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-0.5));
}

TEST_CASE("game_tick: compute.rate is -2.5 after tick with 5 active robots", "[tick][compute][build32]") {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-2.5));
}

TEST_CASE("game_tick: compute.rate is 0.0 after tick with no robots", "[tick][compute][build32]") {
    GameState s = game_init();
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(0.0));
}

// ===========================================================================
// Section 6 — Scrap rate still reads pre-drain compute (ordering preserved)
//
// Step 4 (scrap rate) reads compute.value BEFORE step 5 (advance/drain).
// So even with robots draining compute, the scrap rate is based on the current
// compute buffer, not the post-drain value.
// ===========================================================================

TEST_CASE("game_tick: scrap rate with 2 robots uses pre-drain compute of 10", "[tick][scrap][build32]") {
    // robots=2, compute=10: scrap rate = 2*(0.1 + 10*0.02) = 0.6
    // After advance: compute = clamp(10 + (-1.0)*1, 0, 50) = 9.0  (2 robots * 0.5 = 1.0/s drain)
    // Scrap value after 1s = 0.6
    GameState s = game_init();
    s.resources.at("robots").value  = 2.0;
    s.resources.at("compute").value = 10.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate  == Catch::Approx(0.6));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.6));
    REQUIRE(s.resources.at("compute").value    == Catch::Approx(9.0));
}

TEST_CASE("game_tick: scrap rate with 1 robot uses pre-drain compute of 0", "[tick][scrap][build32]") {
    // robots=1, compute=0: scrap rate = 1*(0.1 + 0*0.02) = 0.1
    // After advance: compute = clamp(0 + (-1)*1, 0, 50) = 0.0 (clamped)
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate  == Catch::Approx(0.1));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.1));
    REQUIRE(s.resources.at("compute").value    == Catch::Approx(0.0));
}
