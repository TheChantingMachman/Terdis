// Build 46 — test_build46.cpp
// Tests for game_over guard on all 12 player-action functions.
//
// Scope: all player-action functions must return false/0.0 immediately when
//        game_over == true, with NO state mutation.
//
// Functions under test:
//   player_click, wake_first_robot, buy_robot, apply_scrap_boost,
//   buy_solar_panel, buy_colony_unit, buy_compute_amplifier, awaken_human,
//   buy_scrap_vault, buy_robot_capacity, build_fabricator, build_reactor
//
// Test design:
//   - Each function is set up so it WOULD succeed if game_over were false
//   - game_over is then set to true
//   - The test verifies the return value AND the absence of state mutation
//
// All values derived from spec/core-spec.md.

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helper: count occurrences of an event id in pending_events
// ---------------------------------------------------------------------------

static int count_event46(const GameState& s, const std::string& id) {
    int n = 0;
    for (const auto& ev : s.pending_events)
        if (ev == id) n++;
    return n;
}

// ===========================================================================
// Section 1 — player_click
// ===========================================================================

TEST_CASE("player_click: returns 0.0 immediately when game_over=true", "[game_over][click][build46]") {
    GameState s = game_init();
    s.resources.at("power").value = 50.0;   // click_cost=1.0 → would succeed
    s.game_over = true;
    REQUIRE(player_click(s) == Catch::Approx(0.0));
}

TEST_CASE("player_click: does not deduct power when game_over=true", "[game_over][click][build46]") {
    GameState s = game_init();
    s.resources.at("power").value = 50.0;
    s.game_over = true;
    double power_before = s.resources.at("power").value;
    player_click(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(power_before));
}

TEST_CASE("player_click: does not add compute when game_over=true", "[game_over][click][build46]") {
    GameState s = game_init();
    s.resources.at("power").value = 50.0;
    s.game_over = true;
    double compute_before = s.resources.at("compute").value;
    player_click(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(compute_before));
}

// ===========================================================================
// Section 2 — wake_first_robot
// ===========================================================================

TEST_CASE("wake_first_robot: returns false immediately when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 15.0;  // >= WAKE_ROBOT_COMPUTE_COST (10.0) → would succeed
    s.first_robot_awoken = false;
    s.game_over = true;
    REQUIRE(wake_first_robot(s) == false);
}

TEST_CASE("wake_first_robot: does not deduct compute when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 15.0;
    s.first_robot_awoken = false;
    s.game_over = true;
    double compute_before = s.resources.at("compute").value;
    wake_first_robot(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(compute_before));
}

TEST_CASE("wake_first_robot: does not set first_robot_awoken when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 15.0;
    s.first_robot_awoken = false;
    s.game_over = true;
    wake_first_robot(s);
    REQUIRE(s.first_robot_awoken == false);
}

TEST_CASE("wake_first_robot: does not change robots.value when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 15.0;
    s.first_robot_awoken = false;
    s.game_over = true;
    double robots_before = s.resources.at("robots").value;
    wake_first_robot(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(robots_before));
}

// ===========================================================================
// Section 3 — buy_robot
// ===========================================================================

TEST_CASE("buy_robot: returns false immediately when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.first_robot_awoken = true;
    s.resources.at("robots").value = 1.0;   // cost = 10 + 2*(1-1) = 10.0
    s.resources.at("tech_scrap").value = 50.0;  // more than enough → would succeed
    s.game_over = true;
    REQUIRE(buy_robot(s) == false);
}

TEST_CASE("buy_robot: does not deduct tech_scrap when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.first_robot_awoken = true;
    s.resources.at("robots").value = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_robot(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_robot: does not increment robots.value when game_over=true", "[game_over][robot][build46]") {
    GameState s = game_init();
    s.first_robot_awoken = true;
    s.resources.at("robots").value = 1.0;
    s.resources.at("tech_scrap").value = 50.0;
    s.game_over = true;
    buy_robot(s);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(1.0));
}

// ===========================================================================
// Section 4 — apply_scrap_boost
// ===========================================================================

TEST_CASE("apply_scrap_boost: returns false immediately when game_over=true", "[game_over][boost][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;  // >= SCRAP_BOOST_COMPUTE_COST (5.0) → would succeed
    s.game_over = true;
    REQUIRE(apply_scrap_boost(s) == false);
}

TEST_CASE("apply_scrap_boost: does not deduct compute when game_over=true", "[game_over][boost][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    s.game_over = true;
    double compute_before = s.resources.at("compute").value;
    apply_scrap_boost(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(compute_before));
}

TEST_CASE("apply_scrap_boost: does not increase scrap_boost when game_over=true", "[game_over][boost][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 10.0;
    s.scrap_boost = 0.0;
    s.game_over = true;
    apply_scrap_boost(s);
    REQUIRE(s.scrap_boost == Catch::Approx(0.0));
}

// ===========================================================================
// Section 5 — buy_solar_panel
// ===========================================================================

TEST_CASE("buy_solar_panel: returns false immediately when game_over=true", "[game_over][solar][build46]") {
    GameState s = game_init();
    s.solar_panel_count = 0;
    s.resources.at("tech_scrap").value = 100.0;  // > SOLAR_BASE_COST (5.0) → would succeed
    s.game_over = true;
    REQUIRE(buy_solar_panel(s) == false);
}

TEST_CASE("buy_solar_panel: does not deduct tech_scrap when game_over=true", "[game_over][solar][build46]") {
    GameState s = game_init();
    s.solar_panel_count = 0;
    s.resources.at("tech_scrap").value = 100.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_solar_panel(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_solar_panel: does not increment solar_panel_count when game_over=true", "[game_over][solar][build46]") {
    GameState s = game_init();
    s.solar_panel_count = 0;
    s.resources.at("tech_scrap").value = 100.0;
    s.game_over = true;
    buy_solar_panel(s);
    REQUIRE(s.solar_panel_count == 0);
}

// ===========================================================================
// Section 6 — buy_colony_unit
// ===========================================================================

TEST_CASE("buy_colony_unit: returns false immediately when game_over=true", "[game_over][colony][build46]") {
    GameState s = game_init();
    s.colony_unit_count = 0;
    s.resources.at("tech_scrap").value = 500.0;  // >= 200.0 → would succeed
    s.game_over = true;
    REQUIRE(buy_colony_unit(s) == false);
}

TEST_CASE("buy_colony_unit: does not deduct tech_scrap when game_over=true", "[game_over][colony][build46]") {
    GameState s = game_init();
    s.colony_unit_count = 0;
    s.resources.at("tech_scrap").value = 500.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_colony_unit(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_colony_unit: does not increment colony_unit_count when game_over=true", "[game_over][colony][build46]") {
    GameState s = game_init();
    s.colony_unit_count = 0;
    s.resources.at("tech_scrap").value = 500.0;
    s.game_over = true;
    buy_colony_unit(s);
    REQUIRE(s.colony_unit_count == 0);
}

TEST_CASE("buy_colony_unit: does not change colony_capacity.value when game_over=true", "[game_over][colony][build46]") {
    GameState s = game_init();
    s.colony_unit_count = 0;
    s.resources.at("tech_scrap").value = 500.0;
    s.game_over = true;
    double cap_before = s.resources.at("colony_capacity").value;
    buy_colony_unit(s);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(cap_before));
}

// ===========================================================================
// Section 7 — buy_compute_amplifier
// ===========================================================================

TEST_CASE("buy_compute_amplifier: returns false immediately when game_over=true", "[game_over][amplifier][build46]") {
    GameState s = game_init();
    s.compute_amplified = false;
    s.resources.at("tech_scrap").value = 50.0;  // >= COMPUTE_AMP_COST (40.0) → would succeed
    s.game_over = true;
    REQUIRE(buy_compute_amplifier(s) == false);
}

TEST_CASE("buy_compute_amplifier: does not deduct tech_scrap when game_over=true", "[game_over][amplifier][build46]") {
    GameState s = game_init();
    s.compute_amplified = false;
    s.resources.at("tech_scrap").value = 50.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_compute_amplifier: does not set compute_amplified when game_over=true", "[game_over][amplifier][build46]") {
    GameState s = game_init();
    s.compute_amplified = false;
    s.resources.at("tech_scrap").value = 50.0;
    s.game_over = true;
    buy_compute_amplifier(s);
    REQUIRE(s.compute_amplified == false);
}

TEST_CASE("buy_compute_amplifier: does not change compute.cap when game_over=true", "[game_over][amplifier][build46]") {
    GameState s = game_init();
    s.compute_amplified = false;
    s.resources.at("tech_scrap").value = 50.0;
    s.game_over = true;
    double cap_before = s.resources.at("compute").cap;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(cap_before));
}

// ===========================================================================
// Section 8 — awaken_human
// ===========================================================================

TEST_CASE("awaken_human: returns false immediately when game_over=true", "[game_over][awaken][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.stasis_pod_count = 10;
    s.awoken_humans = 0;
    s.resources.at("colony_capacity").value = 4.0;  // > awoken_humans → slot available
    s.game_over = true;
    REQUIRE(awaken_human(s) == false);
}

TEST_CASE("awaken_human: does not decrement stasis_pod_count when game_over=true", "[game_over][awaken][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.stasis_pod_count = 10;
    s.awoken_humans = 0;
    s.resources.at("colony_capacity").value = 4.0;
    s.game_over = true;
    awaken_human(s);
    REQUIRE(s.stasis_pod_count == 10);
}

TEST_CASE("awaken_human: does not increment awoken_humans when game_over=true", "[game_over][awaken][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.stasis_pod_count = 10;
    s.awoken_humans = 0;
    s.resources.at("colony_capacity").value = 4.0;
    s.game_over = true;
    awaken_human(s);
    REQUIRE(s.awoken_humans == 0);
}

TEST_CASE("awaken_human: does not push human_awakened event when game_over=true", "[game_over][awaken][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.stasis_pod_count = 10;
    s.awoken_humans = 0;
    s.resources.at("colony_capacity").value = 4.0;
    s.pending_events.clear();
    s.game_over = true;
    awaken_human(s);
    REQUIRE(count_event46(s, "human_awakened") == 0);
}

// ===========================================================================
// Section 9 — buy_scrap_vault
// ===========================================================================

TEST_CASE("buy_scrap_vault: returns false immediately when game_over=true", "[game_over][vault][build46]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;  // >= 300.0 → would succeed
    s.game_over = true;
    REQUIRE(buy_scrap_vault(s) == false);
}

TEST_CASE("buy_scrap_vault: does not deduct tech_scrap when game_over=true", "[game_over][vault][build46]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_scrap_vault(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_scrap_vault: does not increment tech_scrap_vault_tier when game_over=true", "[game_over][vault][build46]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    s.game_over = true;
    buy_scrap_vault(s);
    REQUIRE(s.tech_scrap_vault_tier == 0);
}

TEST_CASE("buy_scrap_vault: does not expand tech_scrap.cap when game_over=true", "[game_over][vault][build46]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    s.game_over = true;
    double cap_before = s.resources.at("tech_scrap").cap;
    buy_scrap_vault(s);
    REQUIRE(s.resources.at("tech_scrap").cap == Catch::Approx(cap_before));
}

TEST_CASE("buy_scrap_vault: does not push vault_expanded event when game_over=true", "[game_over][vault][build46]") {
    GameState s = game_init();
    s.tech_scrap_vault_tier = 0;
    s.resources.at("tech_scrap").value = 400.0;
    s.pending_events.clear();
    s.game_over = true;
    buy_scrap_vault(s);
    REQUIRE(count_event46(s, "vault_expanded") == 0);
}

// ===========================================================================
// Section 10 — buy_robot_capacity
// ===========================================================================

TEST_CASE("buy_robot_capacity: returns false immediately when game_over=true", "[game_over][robot_cap][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap   = 5.0;
    s.resources.at("tech_scrap").value = 600.0;  // >= 500.0 → would succeed
    s.game_over = true;
    REQUIRE(buy_robot_capacity(s) == false);
}

TEST_CASE("buy_robot_capacity: does not deduct tech_scrap when game_over=true", "[game_over][robot_cap][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap   = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    buy_robot_capacity(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("buy_robot_capacity: does not set robots_cap_expanded when game_over=true", "[game_over][robot_cap][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap   = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    s.game_over = true;
    buy_robot_capacity(s);
    REQUIRE(s.robots_cap_expanded == false);
}

TEST_CASE("buy_robot_capacity: does not change robots.cap when game_over=true", "[game_over][robot_cap][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap   = 5.0;
    s.resources.at("tech_scrap").value = 600.0;
    s.game_over = true;
    buy_robot_capacity(s);
    REQUIRE(s.resources.at("robots").cap == Catch::Approx(5.0));
}

// ===========================================================================
// Section 11 — build_fabricator
// ===========================================================================

TEST_CASE("build_fabricator: returns false immediately when game_over=true", "[game_over][fabricator][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap   = 2000.0;
    s.resources.at("tech_scrap").value = 1500.0;  // >= FABRICATOR_COST (1000.0) → would succeed
    s.fabricator_built = false;
    s.game_over = true;
    REQUIRE(build_fabricator(s) == false);
}

TEST_CASE("build_fabricator: does not deduct tech_scrap when game_over=true", "[game_over][fabricator][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap   = 2000.0;
    s.resources.at("tech_scrap").value = 1500.0;
    s.game_over = true;
    double scrap_before = s.resources.at("tech_scrap").value;
    build_fabricator(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(scrap_before));
}

TEST_CASE("build_fabricator: does not set fabricator_built when game_over=true", "[game_over][fabricator][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap   = 2000.0;
    s.resources.at("tech_scrap").value = 1500.0;
    s.game_over = true;
    build_fabricator(s);
    REQUIRE(s.fabricator_built == false);
}

TEST_CASE("build_fabricator: does not push fabricator_online event when game_over=true", "[game_over][fabricator][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap   = 2000.0;
    s.resources.at("tech_scrap").value = 1500.0;
    s.pending_events.clear();
    s.game_over = true;
    build_fabricator(s);
    REQUIRE(count_event46(s, "fabricator_online") == 0);
}

// ===========================================================================
// Section 12 — build_reactor
// ===========================================================================

TEST_CASE("build_reactor: returns false immediately when game_over=true", "[game_over][reactor][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;  // >= REACTOR_CHARGE_COST (80.0) → would succeed
    s.reactor_built = false;
    s.game_over = true;
    REQUIRE(build_reactor(s) == false);
}

TEST_CASE("build_reactor: does not deduct power.value when game_over=true", "[game_over][reactor][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    s.game_over = true;
    double power_before = s.resources.at("power").value;
    build_reactor(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(power_before));
}

TEST_CASE("build_reactor: does not set reactor_built when game_over=true", "[game_over][reactor][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    s.game_over = true;
    build_reactor(s);
    REQUIRE(s.reactor_built == false);
}

TEST_CASE("build_reactor: does not push reactor_online event when game_over=true", "[game_over][reactor][build46]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    s.pending_events.clear();
    s.game_over = true;
    build_reactor(s);
    REQUIRE(count_event46(s, "reactor_online") == 0);
}

// ===========================================================================
// Section 13 — Cross-function: game_over guard priority is unconditional
//
// The guard must fire even when other normal preconditions for failure exist.
// This ensures the guard is checked FIRST, before any other validation.
// ===========================================================================

TEST_CASE("player_click: game_over guard fires even when power is sufficient (guard is first)", "[game_over][priority][build46]") {
    // This test distinguishes game_over guard from power-insufficient guard.
    // With game_over=true and plenty of power, the return value must still be 0.0
    GameState s = game_init();
    s.resources.at("power").value = 90.0;
    s.game_over = true;
    // If guard is second, implementation might still check power and return something
    REQUIRE(player_click(s) == Catch::Approx(0.0));
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("wake_first_robot: game_over guard fires even when compute threshold met", "[game_over][priority][build46]") {
    GameState s = game_init();
    s.resources.at("compute").value = 20.0;  // well above threshold
    s.first_robot_awoken = false;
    s.game_over = true;
    REQUIRE(wake_first_robot(s) == false);
    REQUIRE(s.first_robot_awoken == false);
}

TEST_CASE("buy_solar_panel: game_over guard fires even when panel_count=0 and scrap abundant", "[game_over][priority][build46]") {
    GameState s = game_init();
    s.solar_panel_count = 0;
    s.resources.at("tech_scrap").value = 1000.0;
    s.game_over = true;
    REQUIRE(buy_solar_panel(s) == false);
    REQUIRE(s.solar_panel_count == 0);
}

TEST_CASE("awaken_human: game_over guard fires even in Phase 2 with available colony slot", "[game_over][priority][build46]") {
    // All conditions for awaken_human to succeed are met; game_over blocks it
    GameState s = game_init();
    s.current_phase = 2;
    s.stasis_pod_count = 50;
    s.awoken_humans = 0;
    s.resources.at("colony_capacity").value = 10.0;
    s.game_over = true;
    REQUIRE(awaken_human(s) == false);
    REQUIRE(s.stasis_pod_count == 50);
    REQUIRE(s.awoken_humans == 0);
}

TEST_CASE("buy_robot_capacity: game_over guard fires even when all Phase 2 conditions satisfied", "[game_over][priority][build46]") {
    GameState s = game_init();
    s.current_phase = 2;
    s.resources.at("robots").value = 5.0;
    s.resources.at("robots").cap   = 5.0;
    s.resources.at("tech_scrap").value = 1000.0;
    s.robots_cap_expanded = false;
    s.game_over = true;
    REQUIRE(buy_robot_capacity(s) == false);
    REQUIRE(s.resources.at("robots").cap == Catch::Approx(5.0));
    REQUIRE(s.robots_cap_expanded == false);
}

// ===========================================================================
// Section 14 — No pending_events pushed by any action when game_over=true
//
// Consolidates event-silence requirements across all action functions.
// ===========================================================================

TEST_CASE("no action function pushes events when game_over=true", "[game_over][events][build46]") {
    // Run every action function with game_over=true and prerequisites otherwise met.
    // Verify pending_events remains empty throughout.

    // --- player_click ---
    {
        GameState s = game_init();
        s.resources.at("power").value = 50.0;
        s.game_over = true;
        s.pending_events.clear();
        player_click(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- wake_first_robot ---
    {
        GameState s = game_init();
        s.resources.at("compute").value = 15.0;
        s.first_robot_awoken = false;
        s.game_over = true;
        s.pending_events.clear();
        wake_first_robot(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- buy_robot ---
    {
        GameState s = game_init();
        s.first_robot_awoken = true;
        s.resources.at("robots").value = 1.0;
        s.resources.at("tech_scrap").value = 50.0;
        s.game_over = true;
        s.pending_events.clear();
        buy_robot(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- apply_scrap_boost ---
    {
        GameState s = game_init();
        s.resources.at("compute").value = 10.0;
        s.game_over = true;
        s.pending_events.clear();
        apply_scrap_boost(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- buy_solar_panel ---
    {
        GameState s = game_init();
        s.resources.at("tech_scrap").value = 100.0;
        s.game_over = true;
        s.pending_events.clear();
        buy_solar_panel(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- buy_colony_unit ---
    {
        GameState s = game_init();
        s.resources.at("tech_scrap").value = 500.0;
        s.game_over = true;
        s.pending_events.clear();
        buy_colony_unit(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- buy_compute_amplifier ---
    {
        GameState s = game_init();
        s.resources.at("tech_scrap").value = 50.0;
        s.game_over = true;
        s.pending_events.clear();
        buy_compute_amplifier(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- awaken_human ---
    {
        GameState s = game_init();
        s.current_phase = 2;
        s.stasis_pod_count = 10;
        s.resources.at("colony_capacity").value = 4.0;
        s.game_over = true;
        s.pending_events.clear();
        awaken_human(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- buy_scrap_vault ---
    {
        GameState s = game_init();
        s.resources.at("tech_scrap").value = 400.0;
        s.game_over = true;
        s.pending_events.clear();
        buy_scrap_vault(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- buy_robot_capacity ---
    {
        GameState s = game_init();
        s.current_phase = 2;
        s.resources.at("robots").value = 5.0;
        s.resources.at("robots").cap   = 5.0;
        s.resources.at("tech_scrap").value = 600.0;
        s.game_over = true;
        s.pending_events.clear();
        buy_robot_capacity(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- build_fabricator ---
    {
        GameState s = game_init();
        s.current_phase = 3;
        s.resources.at("tech_scrap").cap   = 2000.0;
        s.resources.at("tech_scrap").value = 1500.0;
        s.game_over = true;
        s.pending_events.clear();
        build_fabricator(s);
        REQUIRE(s.pending_events.empty());
    }

    // --- build_reactor ---
    {
        GameState s = game_init();
        s.current_phase = 3;
        s.resources.at("power").value = 100.0;
        s.game_over = true;
        s.pending_events.clear();
        build_reactor(s);
        REQUIRE(s.pending_events.empty());
    }
}
