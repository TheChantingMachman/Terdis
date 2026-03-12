// Build 24 — test_buy_robot_build24.cpp
// Tests for buy_robot(), current_robot_cost(), all_robots_online milestone.
// All values derived from spec/core-spec.md (Additional Robots, Milestones sections).
//
// New functions:
//   bool   buy_robot(GameState& state)
//   double current_robot_cost(const GameState& state)
//
// Constants:
//   ROBOT_BASE_COST  = 10.0
//   ROBOT_COST_STEP  = 2.0
//   robots.cap       = 5.0
//
// Formula: current_robot_cost = ROBOT_BASE_COST + ROBOT_COST_STEP * (robots.value - 1.0)
// Cost based on current robot count BEFORE purchase. Linear scale.
// Example costs: 2nd=10, 3rd=12, 4th=14, 5th=16

#include <catch2/catch_all.hpp>
#include <algorithm>
#include "custodian.hpp"

// Helper: find milestone by id
static Milestone* find_milestone(GameState& s, const std::string& id) {
    for (auto& m : s.milestones) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

// ===========================================================================
// Section 1 — current_robot_cost
// ===========================================================================

TEST_CASE("current_robot_cost at robots.value=1 returns 10.0", "[buy_robot][cost]") {
    // 10.0 + 2.0 * (1.0 - 1.0) = 10.0
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    REQUIRE(current_robot_cost(s) == Catch::Approx(10.0));
}

TEST_CASE("current_robot_cost at robots.value=2 returns 12.0", "[buy_robot][cost]") {
    // 10.0 + 2.0 * (2.0 - 1.0) = 12.0
    GameState s = game_init();
    s.resources.at("robots").value = 2.0;
    REQUIRE(current_robot_cost(s) == Catch::Approx(12.0));
}

TEST_CASE("current_robot_cost at robots.value=3 returns 14.0", "[buy_robot][cost]") {
    // 10.0 + 2.0 * (3.0 - 1.0) = 14.0
    GameState s = game_init();
    s.resources.at("robots").value = 3.0;
    REQUIRE(current_robot_cost(s) == Catch::Approx(14.0));
}

TEST_CASE("current_robot_cost at robots.value=4 returns 16.0", "[buy_robot][cost]") {
    // 10.0 + 2.0 * (4.0 - 1.0) = 16.0
    GameState s = game_init();
    s.resources.at("robots").value = 4.0;
    REQUIRE(current_robot_cost(s) == Catch::Approx(16.0));
}

TEST_CASE("current_robot_cost increases monotonically", "[buy_robot][cost]") {
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    double c1 = current_robot_cost(s);
    s.resources.at("robots").value = 2.0;
    double c2 = current_robot_cost(s);
    REQUIRE(c1 > 0.0);
    REQUIRE(c2 > c1);
}

TEST_CASE("current_robot_cost does not modify state", "[buy_robot][cost]") {
    GameState s = game_init();
    s.resources.at("robots").value = 3.0;
    double cost = current_robot_cost(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(3.0));
    REQUIRE(cost == Catch::Approx(14.0)); // 10.0 + 2.0*(3.0-1.0) = 14.0
}

// ===========================================================================
// Section 2 — buy_robot guard conditions
// ===========================================================================

TEST_CASE("buy_robot returns false when first_robot_awoken is false", "[buy_robot][guard]") {
    GameState s = game_init();
    // first_robot_awoken starts false; give plenty of scrap
    s.resources.at("tech_scrap").value = 100.0;
    REQUIRE(buy_robot(s) == false);
}

TEST_CASE("buy_robot does not change state when first_robot_awoken is false", "[buy_robot][guard]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 100.0;
    double scrap_before  = s.resources.at("tech_scrap").value;
    double robots_before = s.resources.at("robots").value;
    buy_robot(s);
    REQUIRE(s.resources.at("robots").value    == Catch::Approx(robots_before));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_robot returns false when robots at cap (5.0)", "[buy_robot][guard]") {
    GameState s = game_init();
    s.first_robot_awoken = true;
    s.resources.at("robots").value     = 5.0;
    s.resources.at("tech_scrap").value = 200.0;
    REQUIRE(buy_robot(s) == false);
}

TEST_CASE("buy_robot does not change state when at cap", "[buy_robot][guard]") {
    GameState s = game_init();
    s.first_robot_awoken = true;
    s.resources.at("robots").value     = 5.0;
    s.resources.at("tech_scrap").value = 200.0;
    buy_robot(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(200.0));
    REQUIRE(s.resources.at("robots").value     == Catch::Approx(5.0));
}

TEST_CASE("buy_robot returns false when tech_scrap insufficient", "[buy_robot][guard]") {
    // robots.value=1, cost=10.0; provide 9.9 — just below threshold
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 9.9;
    REQUIRE(buy_robot(s) == false);
}

TEST_CASE("buy_robot does not change state when cannot afford", "[buy_robot][guard]") {
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 9.9;
    buy_robot(s);
    REQUIRE(s.resources.at("robots").value     == Catch::Approx(1.0));
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(9.9));
}

// ===========================================================================
// Section 3 — buy_robot success path
// Setup: first_robot_awoken=true, robots.value=1.0 (first robot already awoken)
// ===========================================================================

TEST_CASE("buy_robot succeeds when affordable and below cap", "[buy_robot][success]") {
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    REQUIRE(buy_robot(s) == true);
}

TEST_CASE("buy_robot deducts current_robot_cost from tech_scrap", "[buy_robot][success]") {
    // robots=1, cost=10.0; tech_scrap=50.0 → 50 - 10 = 40.0
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    buy_robot(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(40.0));
}

TEST_CASE("buy_robot increments robots.value by 1.0", "[buy_robot][success]") {
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    buy_robot(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(2.0));
}

TEST_CASE("buy_robot returns true on success", "[buy_robot][success]") {
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    REQUIRE(buy_robot(s) == true);
}

TEST_CASE("buy_robot calls recompute_rates: power.rate reflects new robot drain", "[buy_robot][success]") {
    // After buying 2nd robot: robots=2, no panels, stasis_drain=0.05
    // power.rate = BASE_POWER_RATE + 0*SOLAR_POWER_RATE - 2*ROBOT_POWER_DRAIN - stasis_drain.value
    //           = 0.1 + 0 - 2*0.2 - 0.05 = 0.1 - 0.4 - 0.05 = -0.35
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    // solar_panel_count=0, stasis_drain.value=0.05 (from game_init)
    buy_robot(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.35));
}

TEST_CASE("buy_robot clamps robots.value at cap 5.0", "[buy_robot][success]") {
    // robots=4 → buy → would be 5 (equals cap) — must be clamped to 5.0
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 4.0;
    s.resources.at("tech_scrap").value = 200.0; // 4th robot costs 16.0; 200.0 is enough
    bool result = buy_robot(s);
    REQUIRE(result == true);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(5.0));
}

TEST_CASE("buy_robot second purchase uses updated cost", "[buy_robot][success]") {
    // robots=1; first buy: cost=10 (robots 1→2); second buy: cost=12 (robots 2→3)
    // tech_scrap=100 → 100 - 10 - 12 = 78.0; robots.value=3.0
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 100.0;
    buy_robot(s); // cost 10 → robots=2, scrap=90
    buy_robot(s); // cost 12 → robots=3, scrap=78
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(78.0));
    REQUIRE(s.resources.at("robots").value     == Catch::Approx(3.0));
}

TEST_CASE("buy_robot at exactly the cost boundary succeeds", "[buy_robot][success]") {
    // robots=1, cost=10.0; tech_scrap=10.0 exactly
    GameState s = game_init();
    s.first_robot_awoken               = true;
    s.resources.at("robots").value     = 1.0;
    s.resources.at("tech_scrap").value = 10.0;
    REQUIRE(buy_robot(s) == true);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.0));
}

// ===========================================================================
// Section 4 — buy_robot integration with game_tick
// ===========================================================================

TEST_CASE("game_tick with 2 robots: scrap rate uses robots.value=2", "[buy_robot][tick]") {
    // tech_scrap.rate = robots.value * (BASE_SCRAP_RATE + compute.value * COMPUTE_SCRAP_BONUS)
    //                 = 2.0 * (0.1 + 0.0 * 0.02) = 2.0 * 0.1 = 0.2
    GameState s = game_init();
    s.first_robot_awoken           = true;
    s.resources.at("robots").value = 2.0;
    s.resources.at("compute").value = 0.0;
    game_tick(s, 1.0);
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.2));
}

TEST_CASE("game_tick with 5 robots: power.rate reflects full drain", "[buy_robot][tick]") {
    // power.rate = BASE_POWER_RATE + 0*SOLAR_POWER_RATE - 5*ROBOT_POWER_DRAIN - stasis_drain.value
    //           = 0.1 + 0 - 5*0.2 - 0.05 = 0.1 - 1.0 - 0.05 = -0.95
    GameState s = game_init();
    s.first_robot_awoken           = true;
    s.resources.at("robots").value = 5.0;
    // solar_panel_count=0, stasis_drain.value=0.05 (total_time=0, stays 0.05 at t=0)
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.95));
}

// ===========================================================================
// Section 5 — all_robots_online milestone
// ===========================================================================

TEST_CASE("check_milestones: all_robots_online not triggered at robots=4.0", "[buy_robot][milestone]") {
    GameState s = game_init();
    s.resources.at("robots").value = 4.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "all_robots_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
}

TEST_CASE("check_milestones: all_robots_online triggers at exactly robots=5.0", "[buy_robot][milestone]") {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "all_robots_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

TEST_CASE("check_milestones: all_robots_online fires pending event at robots=5.0", "[buy_robot][milestone]") {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    check_milestones(s);
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "all_robots_online")
                 != s.pending_events.end();
    REQUIRE(found);
}

TEST_CASE("check_milestones: all_robots_online triggers above 5.0", "[buy_robot][milestone]") {
    // robots.cap=5.0 so value won't exceed 5, but test with value set directly to 5.0
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "all_robots_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

TEST_CASE("game_tick fires all_robots_online when robots reach 5.0", "[buy_robot][milestone]") {
    GameState s = game_init();
    s.resources.at("robots").value = 5.0;
    game_tick(s, 0.0);
    bool found = std::find(s.pending_events.begin(), s.pending_events.end(), "all_robots_online")
                 != s.pending_events.end();
    REQUIRE(found);
}

// ===========================================================================
// Section 6 — save/load round-trip for all_robots_online milestone
// ===========================================================================

TEST_CASE("round-trip: all_robots_online milestone condition fields preserved", "[buy_robot][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "all_robots_online") {
            found = true;
            REQUIRE(m.condition_resource == "robots");
            REQUIRE(m.condition_value    == Catch::Approx(5.0));
            REQUIRE(m.condition_field    == ConditionField::VALUE);
        }
    }
    REQUIRE(found);
}
