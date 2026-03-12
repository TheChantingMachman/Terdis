// Build 3 — test_rates_and_costs.cpp
// Tests for recompute_rates() and current_solar_cost().
// All values derived from spec/core-spec.md.
//
// Constants (from spec):
//   BASE_POWER_RATE    = 0.1
//   SOLAR_POWER_RATE   = 0.75
//   ROBOT_POWER_DRAIN  = 0.2
//   STASIS_DRAIN_BASE  = 0.05 (initial stasis_drain.value)
//   SOLAR_BASE_COST  = 5.0
//   SOLAR_COST_SCALE = 1.10

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ===========================================================================
// current_solar_cost
// Formula: SOLAR_BASE_COST * pow(SOLAR_COST_SCALE, solar_panel_count)
// ===========================================================================

TEST_CASE("current_solar_cost at count=0 returns 5.0", "[cost]") {
    GameState s = game_init();
    REQUIRE(s.solar_panel_count == 0);
    REQUIRE(current_solar_cost(s) == Catch::Approx(5.0));
}

TEST_CASE("current_solar_cost at count=1 returns 5.25", "[cost]") {
    // 5.0 * 1.05^1 = 5.25
    GameState s = game_init();
    s.solar_panel_count = 1;
    REQUIRE(current_solar_cost(s) == Catch::Approx(5.25));
}

TEST_CASE("current_solar_cost at count=2 returns 5.5125", "[cost]") {
    // 5.0 * 1.05^2 = 5.0 * 1.1025 = 5.5125
    GameState s = game_init();
    s.solar_panel_count = 2;
    REQUIRE(current_solar_cost(s) == Catch::Approx(5.5125));
}

TEST_CASE("current_solar_cost at count=3 returns approx 5.788125", "[cost]") {
    // 5.0 * 1.05^3 = 5.0 * 1.157625 = 5.788125
    GameState s = game_init();
    s.solar_panel_count = 3;
    REQUIRE(current_solar_cost(s) == Catch::Approx(5.788125));
}

TEST_CASE("current_solar_cost increases monotonically with count", "[cost]") {
    GameState s = game_init();
    s.solar_panel_count = 0;
    double c0 = current_solar_cost(s);
    s.solar_panel_count = 1;
    double c1 = current_solar_cost(s);
    s.solar_panel_count = 2;
    double c2 = current_solar_cost(s);
    REQUIRE(c1 > c0);
    REQUIRE(c2 > c1);
}

TEST_CASE("current_solar_cost does not modify state", "[cost]") {
    GameState s = game_init();
    s.solar_panel_count = 2;
    current_solar_cost(s);
    REQUIRE(s.solar_panel_count == 2);
}

// ===========================================================================
// recompute_rates
// power.rate = BASE_POWER_RATE + solar_panel_count * SOLAR_POWER_RATE
//              - robots.value * ROBOT_POWER_DRAIN - resources["stasis_drain"].value
// compute.rate = -robots.value * ROBOT_COMPUTE_DRAIN   (0.0 when no robots)
// robots.rate  = 0.0
// tech_scrap.rate = 0.0 (dynamic — reset to 0 here)
// stasis_drain.rate = 0.0
// colony_capacity.rate = 0.0
// At game_init: stasis_drain.value = 0.05
// ===========================================================================

TEST_CASE("recompute_rates: no panels no robots → power.rate = 0.05", "[rates]") {
    // 0.1 + 0 - 0 - 0.05 = 0.05  (stasis_drain.value = 0.05 at init)
    GameState s = game_init();
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.05));
}

TEST_CASE("recompute_rates: 1 panel no robots → power.rate = 0.80", "[rates]") {
    // 0.1 + 1*0.75 - 0*0.2 - 0.05 = 0.80
    GameState s = game_init();
    s.solar_panel_count = 1;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.80));
}

TEST_CASE("recompute_rates: 2 panels no robots → power.rate = 1.55", "[rates]") {
    // 0.1 + 2*0.75 - 0*0.2 - 0.05 = 1.55
    GameState s = game_init();
    s.solar_panel_count = 2;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(1.55));
}

TEST_CASE("recompute_rates: no panels 1 robot → power.rate = -0.15", "[rates]") {
    // 0.1 + 0*0.5 - 1*0.2 - 0.05 = -0.15
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.15));
}

TEST_CASE("recompute_rates: 1 panel 1 robot → power.rate = 0.60", "[rates]") {
    // 0.1 + 1*0.75 - 1*0.2 - 0.05 = 0.60
    GameState s = game_init();
    s.solar_panel_count = 1;
    s.resources.at("robots").value = 1.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.60));
}

TEST_CASE("recompute_rates: 2 panels 1 robot → power.rate = 1.35", "[rates]") {
    // 0.1 + 2*0.75 - 1*0.2 - 0.05 = 1.35
    GameState s = game_init();
    s.solar_panel_count = 2;
    s.resources.at("robots").value = 1.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(1.35));
}

TEST_CASE("recompute_rates sets compute.rate based on ROBOT_COMPUTE_DRAIN formula", "[rates]") {
    // compute.rate = -robots.value * ROBOT_COMPUTE_DRAIN (ROBOT_COMPUTE_DRAIN=0.5)
    // 2 robots * 0.5 = 1.0
    GameState s = game_init();
    s.resources.at("robots").value = 2.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(-1.0));

    // With no robots: compute.rate = 0.0
    s.resources.at("robots").value = 0.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(0.0));
}

TEST_CASE("recompute_rates always sets robots.rate to 0.0", "[rates]") {
    GameState s = game_init();
    s.resources.at("robots").rate = 5.0; // artificially set
    recompute_rates(s);
    REQUIRE(s.resources.at("robots").rate == Catch::Approx(0.0));
}

TEST_CASE("recompute_rates always sets tech_scrap.rate to 0.0", "[rates]") {
    // tech_scrap.rate is dynamic (set by game_tick); recompute_rates resets it
    GameState s = game_init();
    s.resources.at("tech_scrap").rate = 3.0; // artificially set
    recompute_rates(s);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.0));
}

TEST_CASE("recompute_rates does not change resource values, only rates", "[rates]") {
    GameState s = game_init();
    s.resources.at("power").value   = 42.0;
    s.resources.at("compute").value = 7.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").value   == Catch::Approx(42.0));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(7.0));
}

TEST_CASE("recompute_rates always sets stasis_drain.rate to 0.0", "[rates]") {
    GameState s = game_init();
    s.resources.at("stasis_drain").rate = 99.0; // artificially set
    recompute_rates(s);
    REQUIRE(s.resources.at("stasis_drain").rate == Catch::Approx(0.0));
}

TEST_CASE("recompute_rates always sets colony_capacity.rate to 0.0", "[rates]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").rate = 99.0; // artificially set
    recompute_rates(s);
    REQUIRE(s.resources.at("colony_capacity").rate == Catch::Approx(0.0));
}
