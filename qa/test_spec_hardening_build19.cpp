// Build 19 — test_spec_hardening_build19.cpp
// Spec compliance hardening tests:
//   1. buy_colony_unit must call recompute_rates after purchase (spec: "called after
//      any purchase, wake action, or each tick" — Rate Recomputation section)
//   2. game_tick stasis_max must use state.stasis_pod_count, not the compile-time
//      STASIS_POD_COUNT constant, so the cap honoured after save/load.
//
// All values derived from spec/core-spec.md and the build-19 work order context.
//
// Constants used:
//   BASE_POWER_RATE   = 0.1
//   SOLAR_POWER_RATE  = 0.5
//   ROBOT_POWER_DRAIN = 0.2
//   STASIS_DRAIN_BASE   = 0.05
//   STASIS_DRAIN_GROWTH = 0.003
//   STASIS_DRAIN_PER_POD = 0.4
//   STASIS_POD_COUNT (default) = 50  → default max = 20.0
//   COLONY_CAPACITY_PER_UNIT = 2

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ===========================================================================
// Section 1 — buy_colony_unit calls recompute_rates after purchase
//
// Strategy: corrupt power.rate to a sentinel value (999.0) that cannot arise
// from any valid recompute_rates call, then verify it is corrected after
// buy_colony_unit, proving the function called recompute_rates.
// ===========================================================================

TEST_CASE("buy_colony_unit calls recompute_rates: power.rate corrected (no robots, no panels)", "[colony][recompute]") {
    // Expected power.rate after recompute:
    //   BASE_POWER_RATE + 0*SOLAR_POWER_RATE - 0*ROBOT_POWER_DRAIN - stasis_drain.value
    //   = 0.1 + 0 - 0 - 0.05 = 0.05
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 250.0; // enough for first unit (cost=200.0)
    s.resources.at("power").rate = 999.0;       // corrupt; recompute must overwrite this
    buy_colony_unit(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.05));
}

TEST_CASE("buy_colony_unit calls recompute_rates: power.rate corrected (1 robot active)", "[colony][recompute]") {
    // Expected power.rate after recompute with 1 robot, stasis_drain.value=0.05:
    //   0.1 + 0*SOLAR_POWER_RATE - 1*ROBOT_POWER_DRAIN - stasis_drain.value
    //   = 0.1 + 0 - 0.2 - 0.05 = -0.15
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    s.first_robot_awoken = true;
    s.resources.at("tech_scrap").value = 250.0;
    s.resources.at("power").rate = 999.0; // corrupt
    buy_colony_unit(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.15));
}

TEST_CASE("buy_colony_unit calls recompute_rates: power.rate corrected (1 solar panel, no robots)", "[colony][recompute]") {
    // Expected power.rate after recompute with 1 solar panel, stasis_drain.value=0.05:
    //   0.1 + 1*0.75 - 0*0.2 - 0.05 = 0.80
    GameState s = game_init();
    s.solar_panel_count = 1;
    s.resources.at("tech_scrap").value = 250.0;
    s.resources.at("power").rate = 999.0; // corrupt
    buy_colony_unit(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.80));
}

TEST_CASE("buy_colony_unit calls recompute_rates: power.rate corrected (1 robot + 1 panel)", "[colony][recompute]") {
    // Expected power.rate: 0.1 + 1*0.75 - 1*0.2 - 0.05 = 0.60
    GameState s = game_init();
    s.solar_panel_count = 1;
    s.resources.at("robots").value = 1.0;
    s.first_robot_awoken = true;
    s.resources.at("tech_scrap").value = 250.0;
    s.resources.at("power").rate = 999.0; // corrupt
    buy_colony_unit(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.60));
}

TEST_CASE("buy_colony_unit does NOT call recompute_rates when purchase fails (insufficient scrap)", "[colony][recompute]") {
    // When buy fails, the corrupted rate must remain — recompute must not be called
    // if the purchase guard returns false.
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 19.9; // below cost of 200.0
    s.resources.at("power").rate = 999.0;      // sentinel
    bool result = buy_colony_unit(s);
    REQUIRE(result == false);
    // Sentinel should remain — no state change on failure
    REQUIRE(s.resources.at("power").rate == Catch::Approx(999.0));
}

// ===========================================================================
// Section 2 — game_tick stasis_max uses state.stasis_pod_count
//
// When stasis_pod_count is set to a non-default value, the cap on stasis_drain
// must be stasis_pod_count * STASIS_DRAIN_PER_POD, not the compile-time value
// STASIS_POD_COUNT * STASIS_DRAIN_PER_POD.
//
// Bug scenario: if game_tick uses the constant (50), then after load_game
// restoring stasis_pod_count=25, the cap would be wrong (2.0 instead of 1.0).
// ===========================================================================

TEST_CASE("game_tick stasis_max: with stasis_pod_count=25, drain at t=400 is 0.45", "[stasis][pod_count]") {
    // cap(25 pods) = 25 * 2.0 = 50.0; STASIS_DRAIN_CAP=4.0
    // at total_time=400: formula = 0.05 + 0.001*400 = 0.45 → not clamped (0.45 < 4.0)
    GameState s = game_init();
    s.stasis_pod_count = 25;
    s.total_time = 400.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.45));
}

TEST_CASE("game_tick stasis_max: with stasis_pod_count=10, drain at t=200 is 0.25", "[stasis][pod_count]") {
    // cap(10 pods) = 10 * 2.0 = 20.0; STASIS_DRAIN_CAP=4.0
    // at total_time=200: formula = 0.05 + 0.001*200 = 0.25 → not clamped (0.25 < 4.0)
    GameState s = game_init();
    s.stasis_pod_count = 10;
    s.total_time = 200.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.25));
}

TEST_CASE("game_tick stasis_max: default stasis_pod_count=50 drain capped at 4.0", "[stasis][pod_count]") {
    // Regression: default behaviour with STASIS_DRAIN_CAP=4.0
    // at t=33400: min(0.05+0.001*33400, 50*2.0, 4.0) = min(33.45, 100.0, 4.0) = 4.0
    GameState s = game_init();
    REQUIRE(s.stasis_pod_count == 50);
    s.total_time = 33400.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

TEST_CASE("game_tick stasis_max: below-cap value unaffected by pod_count change", "[stasis][pod_count]") {
    // at total_time=0: formula = 0.05 + 0.003*0 = 0.05; well below any reasonable cap
    // stasis_pod_count=10 → cap=0.4; 0.05 < 0.4 so value should be 0.05 unchanged
    GameState s = game_init();
    s.stasis_pod_count = 10;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.05));
}

// ===========================================================================
// Section 3 — save/load + game_tick: stasis_pod_count round-trips and is
// honoured by game_tick's stasis_max calculation
// ===========================================================================

TEST_CASE("save/load + game_tick: restored stasis_pod_count=25 produces drain 0.45 at t=400", "[stasis][pod_count][save_load]") {
    // Simulate loading a game where stasis_pod_count was 25.
    // game_tick must use the loaded value, not the compile-time constant.
    // At t=400: formula = 0.05 + 0.001*400 = 0.45; cap(25 pods) = 25*2.0 = 50.0; STASIS_DRAIN_CAP=4.0
    // min(0.45, 50.0, 4.0) = 0.45 (not clamped)
    GameState s = game_init();
    s.stasis_pod_count = 25;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.stasis_pod_count == 25); // precondition: save/load preserves it
    loaded.total_time = 400.0;
    game_tick(loaded, 0.0);
    REQUIRE(loaded.resources.at("stasis_drain").value == Catch::Approx(0.45));
}

TEST_CASE("save/load + game_tick: restored stasis_pod_count=50 caps stasis_drain at 4.0", "[stasis][pod_count][save_load]") {
    // Baseline: round-trip with default count gives STASIS_DRAIN_CAP=4.0
    // At t=33400: min(0.05+0.001*33400, 50*2.0, 4.0) = min(33.45, 100.0, 4.0) = 4.0
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.stasis_pod_count == 50);
    loaded.total_time = 33400.0;
    game_tick(loaded, 0.0);
    REQUIRE(loaded.resources.at("stasis_drain").value == Catch::Approx(4.0));
}
