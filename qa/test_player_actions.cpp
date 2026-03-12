// Build 3 — test_player_actions.cpp
// Tests for player_click(), wake_first_robot(), and buy_solar_panel().
// All values derived from spec/core-spec.md.

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ===========================================================================
// player_click
// click_cost=1.0, click_power=1.0
// ===========================================================================

TEST_CASE("player_click returns 0.0 when power < click_cost", "[click]") {
    GameState s = game_init();
    s.resources.at("power").value = 0.5; // below click_cost=1.0
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(0.0));
}

TEST_CASE("player_click does not change power when insufficient", "[click]") {
    GameState s = game_init();
    s.resources.at("power").value = 0.5;
    player_click(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(0.5));
}

TEST_CASE("player_click does not change compute when insufficient power", "[click]") {
    GameState s = game_init();
    s.resources.at("power").value   = 0.5;
    s.resources.at("compute").value = 0.0;
    player_click(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("player_click deducts click_cost from power on success", "[click]") {
    // power starts 15.0, click_cost=1.0 → after click: 14.0
    GameState s = game_init();
    player_click(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(14.0));
}

TEST_CASE("player_click adds click_power to compute on success", "[click]") {
    // compute starts 0.0, click_power=1.0 → after click: 1.0
    GameState s = game_init();
    player_click(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(1.0));
}

TEST_CASE("player_click returns compute actually added on success", "[click]") {
    GameState s = game_init();
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(1.0));
}

TEST_CASE("player_click returns exactly click_cost=1.0 threshold boundary: power==click_cost succeeds", "[click]") {
    GameState s = game_init();
    s.resources.at("power").value = 1.0; // exactly equal to click_cost
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(1.0));
    REQUIRE(s.resources.at("power").value == Catch::Approx(0.0));
}

TEST_CASE("player_click clamps compute at cap when near full", "[click]") {
    // compute cap=50.0; set compute to 49.5; click_power=1.0 → would be 50.5 → clamped to 50.0
    GameState s = game_init();
    s.resources.at("compute").value = 49.5;
    double added = player_click(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(50.0));
    // Returns the amount actually added (0.5), not click_power (1.0)
    REQUIRE(added == Catch::Approx(0.5));
}

TEST_CASE("player_click returns 0.0 when compute already at cap", "[click]") {
    GameState s = game_init();
    s.resources.at("compute").value = 50.0; // at cap
    double added = player_click(s);
    REQUIRE(added == Catch::Approx(0.0));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(50.0));
}

TEST_CASE("player_click successive clicks accumulate compute", "[click]") {
    GameState s = game_init();
    player_click(s); // compute: 0 → 1
    player_click(s); // compute: 1 → 2
    player_click(s); // compute: 2 → 3
    REQUIRE(s.resources.at("compute").value == Catch::Approx(3.0));
    REQUIRE(s.resources.at("power").value   == Catch::Approx(12.0));
}

// ===========================================================================
// wake_first_robot
// WAKE_ROBOT_COMPUTE_COST=10.0
// ===========================================================================

TEST_CASE("wake_first_robot returns false when first_robot_awoken is already true", "[wake]") {
    GameState s = game_init();
    s.first_robot_awoken            = true;
    s.resources.at("compute").value = 20.0; // enough compute
    bool result = wake_first_robot(s);
    REQUIRE(result == false);
}

TEST_CASE("wake_first_robot does nothing when already awoken", "[wake]") {
    GameState s = game_init();
    s.first_robot_awoken            = true;
    s.resources.at("compute").value = 20.0;
    s.resources.at("robots").value  = 0.0;
    wake_first_robot(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(0.0));
    REQUIRE(s.first_robot_awoken == true);
}

TEST_CASE("wake_first_robot returns false when compute < 10.0", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 9.9;
    bool result = wake_first_robot(s);
    REQUIRE(result == false);
}

TEST_CASE("wake_first_robot does not consume compute when insufficient", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 9.9;
    wake_first_robot(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(9.9));
}

TEST_CASE("wake_first_robot does not set robots when compute insufficient", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 9.9;
    wake_first_robot(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(0.0));
}

TEST_CASE("wake_first_robot succeeds at exactly 10.0 compute", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    bool result = wake_first_robot(s);
    REQUIRE(result == true);
}

TEST_CASE("wake_first_robot deducts 10.0 compute on success", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 15.0;
    wake_first_robot(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(5.0));
}

TEST_CASE("wake_first_robot sets robots.value to 1.0 on success", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    wake_first_robot(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(1.0));
}

TEST_CASE("wake_first_robot sets first_robot_awoken to true on success", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    wake_first_robot(s);
    REQUIRE(s.first_robot_awoken == true);
}

TEST_CASE("wake_first_robot returns true on success", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    bool result = wake_first_robot(s);
    REQUIRE(result == true);
}

TEST_CASE("wake_first_robot calls recompute_rates: power rate changes to reflect robot drain", "[wake]") {
    // No solar panels; after wake: power.rate = 0.1 - 1*0.2 - 0.05 = -0.15
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    wake_first_robot(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.15));
}

TEST_CASE("wake_first_robot second call always returns false", "[wake]") {
    GameState s = game_init();
    s.resources.at("compute").value = 30.0;
    wake_first_robot(s); // first call succeeds
    s.resources.at("compute").value = 20.0; // restore compute
    bool second = wake_first_robot(s);
    REQUIRE(second == false);
}

// ===========================================================================
// buy_solar_panel
// SOLAR_BASE_COST=5.0, SOLAR_COST_SCALE=1.10, SOLAR_POWER_RATE=0.75
// ===========================================================================

TEST_CASE("buy_solar_panel returns false when tech_scrap < cost", "[solar]") {
    GameState s = game_init();
    // First panel costs 5.0; tech_scrap starts at 0
    bool result = buy_solar_panel(s);
    REQUIRE(result == false);
}

TEST_CASE("buy_solar_panel does not change state when cannot afford", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 4.9; // below cost of 5.0
    buy_solar_panel(s);
    REQUIRE(s.solar_panel_count == 0);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(4.9));
}

TEST_CASE("buy_solar_panel succeeds at exactly first panel cost (5.0)", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 5.0;
    bool result = buy_solar_panel(s);
    REQUIRE(result == true);
}

TEST_CASE("buy_solar_panel deducts cost from tech_scrap", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 10.0;
    buy_solar_panel(s);
    // cost = 5.0 * 1.10^0 = 5.0 → remaining 5.0
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(5.0));
}

TEST_CASE("buy_solar_panel increments solar_panel_count by 1", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 20.0;
    buy_solar_panel(s);
    REQUIRE(s.solar_panel_count == 1);
}

TEST_CASE("buy_solar_panel calls recompute_rates: power.rate increases by SOLAR_POWER_RATE", "[solar]") {
    // No robots; after 1 panel: power.rate = 0.1 + 1*0.75 - 0.05 = 0.80
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 10.0;
    buy_solar_panel(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.80));
}

TEST_CASE("buy_solar_panel second panel costs 5.25 (5.0 * 1.05^1)", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 20.0;
    buy_solar_panel(s); // first panel: cost 5.0, count→1
    // Second panel cost = 5.0 * 1.05^1 = 5.25
    double before = s.resources.at("tech_scrap").value; // 15.0
    buy_solar_panel(s);
    REQUIRE(s.solar_panel_count == 2);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(before - 5.25));
}

TEST_CASE("buy_solar_panel returns false for second panel when insufficient scrap", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 10.0;
    buy_solar_panel(s); // first panel costs 5.0 → 5.0 remaining
    // second costs 5.25 → cannot afford 5.25 from 5.0
    bool result = buy_solar_panel(s);
    REQUIRE(result == false);
}

TEST_CASE("buy_solar_panel two panels: power.rate = 0.1 + 2*0.75 - 0.05 = 1.55", "[solar]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 15.0;
    buy_solar_panel(s); // panel 1: cost 5.0
    s.resources.at("tech_scrap").value = 10.0; // replenish for clarity
    buy_solar_panel(s); // panel 2: cost 5.50
    // No robots: power.rate = 0.1 + 2*0.75 - 0.05 = 1.55
    REQUIRE(s.resources.at("power").rate == Catch::Approx(1.55));
}
