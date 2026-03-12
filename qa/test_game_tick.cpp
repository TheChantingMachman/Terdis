// Build 3 — test_game_tick.cpp
// Tests for game_tick(): dynamic scrap rate, resource advancement, compute
// reset, total_time accumulation, and integration with phase gates / milestones.
// All values derived from spec/core-spec.md.

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// total_time accumulation
// ---------------------------------------------------------------------------

TEST_CASE("game_tick accumulates total_time by delta", "[tick][time]") {
    GameState s = game_init();
    game_tick(s, 1.0);
    REQUIRE(s.total_time == Catch::Approx(1.0));
}

TEST_CASE("game_tick accumulates total_time across multiple ticks", "[tick][time]") {
    GameState s = game_init();
    game_tick(s, 0.5);
    game_tick(s, 0.5);
    REQUIRE(s.total_time == Catch::Approx(1.0));
}

TEST_CASE("game_tick zero delta does not change total_time", "[tick][time]") {
    GameState s = game_init();
    game_tick(s, 0.0);
    REQUIRE(s.total_time == Catch::Approx(0.0));
}

// ---------------------------------------------------------------------------
// Resource rate application
// ---------------------------------------------------------------------------

TEST_CASE("game_tick advances power by rate*delta", "[tick][resources]") {
    // power: value=15.0; after stasis update (t=0): stasis_drain=0.05;
    // recompute: rate=0.1-0.05=0.05; after 1s → 15.0 + 0.05*1.0 = 15.05
    GameState s = game_init();
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("power").value == Catch::Approx(15.05));
}

TEST_CASE("game_tick advances power correctly for fractional delta", "[tick][resources]") {
    // power: value=15.0; rate=0.05 (stasis_drain=0.05); after 0.5s → 15.0 + 0.05*0.5 = 15.025
    GameState s = game_init();
    game_tick(s, 0.5);
    REQUIRE(s.resources.at("power").value == Catch::Approx(15.025));
}

TEST_CASE("game_tick clamps power at cap", "[tick][resources]") {
    GameState s = game_init();
    s.resources.at("power").value = 99.95;
    // rate=0.1, delta=1.0 → would be 100.05 → clamped to 100.0
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("power").value == Catch::Approx(100.0));
}

TEST_CASE("game_tick clamps power at 0 when rate is negative", "[tick][resources]") {
    GameState s = game_init();
    // game_tick calls recompute_rates, so manually setting rate has no effect.
    // Produce a genuinely negative rate via the engine: 1 robot drains 0.2/s,
    // base rate is 0.1, stasis_drain starts at 0.05 → net = 0.1 - 0.2 - 0.05 = -0.15/s
    s.resources.at("robots").value = 1.0;
    s.first_robot_awoken = true;
    s.resources.at("power").value = 0.05;
    // After 1 tick: 0.05 + (-0.15)*1.0 = -0.10 → clamped to 0.0
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("power").value == Catch::Approx(0.0));
}

TEST_CASE("game_tick does not change compute when robots absent", "[tick][resources]") {
    // robots=0; compute.rate=0; compute should stay at 0
    GameState s = game_init();
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("game_tick does not reset compute to zero when robots absent", "[tick][resources]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    game_tick(s, 1.0);
    // robots=0, so compute is NOT wiped; rate=0 so no change
    REQUIRE(s.resources.at("compute").value == Catch::Approx(5.0));
}

// ---------------------------------------------------------------------------
// Compute drain when robots active
// ---------------------------------------------------------------------------

TEST_CASE("game_tick drains compute by ROBOT_COMPUTE_DRAIN when robots active", "[tick][compute]") {
    // robots=1, compute=8.0, delta=1.0: compute.rate=-0.5 → clamp(8+(-0.5)*1, 0, 50) = 7.5
    GameState s = game_init();
    s.resources.at("compute").value = 8.0;
    s.resources.at("robots").value  = 1.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(7.5));
}

TEST_CASE("game_tick does not reset compute when robots.value == 0", "[tick][compute]") {
    GameState s = game_init();
    s.resources.at("compute").value = 8.0;
    // robots.value is 0 by default
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(8.0));
}

// ---------------------------------------------------------------------------
// Dynamic scrap rate — spec: rate = robots * (BASE_SCRAP_RATE + compute * COMPUTE_SCRAP_BONUS)
// BASE_SCRAP_RATE=0.1, COMPUTE_SCRAP_BONUS=0.02
// ---------------------------------------------------------------------------

TEST_CASE("game_tick sets tech_scrap rate to 0 when no robots", "[tick][scrap]") {
    GameState s = game_init();
    s.resources.at("compute").value = 20.0;
    // robots=0 → scrap rate = 0
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.0));
}

TEST_CASE("game_tick computes scrap rate: 1 robot 0 compute -> 0.1", "[tick][scrap]") {
    // rate = 1 * (0.1 + 0 * 0.02) = 0.1
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.1));
}

TEST_CASE("game_tick computes scrap rate: 1 robot 5 compute -> 0.2", "[tick][scrap]") {
    // rate = 1 * (0.1 + 5 * 0.02) = 0.2
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 5.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.2));
}

TEST_CASE("game_tick computes scrap rate: 1 robot 20 compute -> 0.5", "[tick][scrap]") {
    // rate = 1 * (0.1 + 20 * 0.02) = 0.5
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 20.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.5));
}

TEST_CASE("game_tick scrap rate uses compute value before drain", "[tick][scrap]") {
    // Compute=5, robots=1 → scrap rate computed as 0.2 (from compute=5 before advance)
    // Then advance step drains compute: clamp(5 + (-0.5)*1, 0, 50) = 4.5
    // The rate field and the produced scrap must reflect compute=5, not post-drain value.
    GameState s = game_init();
    s.resources.at("robots").value  = 1.0;
    s.resources.at("compute").value = 5.0;
    game_tick(s, 1.0);
    // rate was set from compute=5, then applied → tech_scrap advances by 0.2 * 1.0 = 0.2
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.2));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.2));
    // compute drained by 1 robot over 1s: 5.0 + (-0.5)*1.0 = 4.5
    REQUIRE(s.resources.at("compute").value == Catch::Approx(4.5));
}

TEST_CASE("game_tick scrap value advances by rate*delta over 2 seconds", "[tick][scrap]") {
    // 1 robot, compute=0 → rate=0.1; 2 ticks of 1s → 0.2
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    game_tick(s, 1.0);
    game_tick(s, 1.0);
    // Each tick: scrap rate = 1*(0.1 + 0*0.02) = 0.1; value += 0.1 each tick
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.2));
}

TEST_CASE("game_tick clamps tech_scrap at cap 500.0", "[tick][scrap]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 499.95;
    s.resources.at("robots").value     = 1.0;
    // rate = 0.1, delta=1.0 → would produce 0.1 → total 500.05 → clamped to 500.0
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(500.0));
}

// ---------------------------------------------------------------------------
// Integration: phase gate triggered during tick
// ---------------------------------------------------------------------------

TEST_CASE("game_tick advances phase when gate conditions met", "[tick][phase]") {
    // New gate: power RATE >= 0.0, colony_capacity VALUE >= 10.0
    // Default state gives power.rate=0.05 >= 0.0; set colony_capacity.value=10.0
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 10.0;
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("game_tick does not advance phase when gate conditions not met", "[tick][phase]") {
    GameState s = game_init();
    // Only power condition met; tech_scrap and robots are 0
    s.resources.at("power").value = 50.0;
    game_tick(s, 0.0);
    REQUIRE(s.current_phase == 1);
}

// ---------------------------------------------------------------------------
// Integration: milestone triggered during tick
// ---------------------------------------------------------------------------

TEST_CASE("game_tick fires first_compute milestone when compute reaches 1.0", "[tick][milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 1.0;
    game_tick(s, 0.0);
    bool found = false;
    for (const auto& ev : s.pending_events) {
        if (ev == "first_compute") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("game_tick does not fire first_compute milestone when compute below threshold", "[tick][milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 0.5;
    game_tick(s, 0.0);
    for (const auto& ev : s.pending_events) {
        REQUIRE(ev != "first_compute");
    }
}
