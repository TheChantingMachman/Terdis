// Build 45 — test_build45.cpp
// Hardening tests for Phase 3 stubs and save/load edge cases.
//
// Scope:
//   1. build_fabricator: tech_scrap cost check and deduction (FABRICATOR_COST=1000.0)
//   2. build_reactor: power.value >= REACTOR_CHARGE_COST guard (REACTOR_CHARGE_COST=80.0)
//   3. build_reactor: deducts REACTOR_CHARGE_COST from power.value on success
//   4. build_reactor: adds permanent power bonus to recompute_rates (REACTOR_POWER_BONUS=10.0)
//   5. Save/load edge cases: game_over=true, combined late-game state
//
// Build44 already covers: phase guard (<3), already-built guard, basic success/failure,
//   event pushes, field initialisation, and basic save/load for fabricator_built,
//   reactor_built, robots_cap_expanded, tech_scrap_vault_tier, solar_decay_accumulator.
//
// These tests add the hardened behaviour introduced in build 45.
//
// Constants (balance TBD per spec; placeholders used here):
//   FABRICATOR_COST      = 1000.0
//   REACTOR_CHARGE_COST  =   80.0
//   REACTOR_POWER_BONUS  =   10.0
//
// All values derived from spec/core-spec.md.

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static int count_event45(const GameState& s, const std::string& id) {
    int n = 0;
    for (const auto& ev : s.pending_events) {
        if (ev == id) n++;
    }
    return n;
}

// ===========================================================================
// Section 1 — build_fabricator: cost check (FABRICATOR_COST = 1000.0)
// ===========================================================================

TEST_CASE("build_fabricator: returns false when tech_scrap < FABRICATOR_COST (1000.0)", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 999.9; // just below threshold
    REQUIRE(build_fabricator(s) == false);
}

TEST_CASE("build_fabricator: returns false when tech_scrap = 0.0 (no scrap)", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").value = 0.0;
    REQUIRE(build_fabricator(s) == false);
}

TEST_CASE("build_fabricator: returns true at exactly FABRICATOR_COST (1000.0) scrap", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 1000.0;
    REQUIRE(build_fabricator(s) == true);
}

TEST_CASE("build_fabricator: deducts FABRICATOR_COST (1000.0) from tech_scrap on success", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 1500.0;
    build_fabricator(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(500.0));
}

TEST_CASE("build_fabricator: does not deduct tech_scrap when cost check fails", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 999.9;
    build_fabricator(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(999.9));
}

TEST_CASE("build_fabricator: does not set fabricator_built when cost check fails", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 500.0; // insufficient
    build_fabricator(s);
    REQUIRE(s.fabricator_built == false);
}

TEST_CASE("build_fabricator: does not push event when cost check fails", "[fabricator][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 500.0;
    s.pending_events.clear();
    build_fabricator(s);
    REQUIRE(count_event45(s, "fabricator_online") == 0);
}

TEST_CASE("build_fabricator: sets fabricator_built=true and deducts cost on success", "[fabricator][cost][build45]") {
    // Combined: both side-effects must occur
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("tech_scrap").value = 1200.0;
    bool result = build_fabricator(s);
    REQUIRE(result == true);
    REQUIRE(s.fabricator_built == true);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(200.0));
}

// ===========================================================================
// Section 2 — build_reactor: power.value guard (REACTOR_CHARGE_COST = 80.0)
// ===========================================================================

TEST_CASE("build_reactor: returns false when power.value < REACTOR_CHARGE_COST (80.0)", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 79.9; // just below threshold
    REQUIRE(build_reactor(s) == false);
}

TEST_CASE("build_reactor: returns false when power.value = 0.0", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 0.0;
    REQUIRE(build_reactor(s) == false);
}

TEST_CASE("build_reactor: returns true at exactly REACTOR_CHARGE_COST (80.0) power", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 80.0;
    REQUIRE(build_reactor(s) == true);
}

TEST_CASE("build_reactor: does not set reactor_built when power.value < charge cost", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 50.0;
    build_reactor(s);
    REQUIRE(s.reactor_built == false);
}

TEST_CASE("build_reactor: does not push event when power.value < charge cost", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 50.0;
    s.pending_events.clear();
    build_reactor(s);
    REQUIRE(count_event45(s, "reactor_online") == 0);
}

// ===========================================================================
// Section 3 — build_reactor: deducts REACTOR_CHARGE_COST from power.value
// ===========================================================================

TEST_CASE("build_reactor: deducts REACTOR_CHARGE_COST (80.0) from power.value on success", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    build_reactor(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(20.0));
}

TEST_CASE("build_reactor: power.value = 80.0 → 0.0 after deduction (exact threshold)", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 80.0;
    build_reactor(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(0.0));
}

TEST_CASE("build_reactor: does not deduct power when charge cost guard fails", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 79.9;
    build_reactor(s);
    REQUIRE(s.resources.at("power").value == Catch::Approx(79.9));
}

TEST_CASE("build_reactor: reactor_built=true and power deducted simultaneously on success", "[reactor][cost][build45]") {
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 90.0;
    bool result = build_reactor(s);
    REQUIRE(result == true);
    REQUIRE(s.reactor_built == true);
    REQUIRE(s.resources.at("power").value == Catch::Approx(10.0));
}

// ===========================================================================
// Section 4 — build_reactor: permanent power bonus via recompute_rates
//
// Spec: "Adds a large permanent BASE_POWER_RATE bonus (flat addition to
//        recompute_rates when reactor_built is true)"
// Placeholder: REACTOR_POWER_BONUS = 10.0
//
// Baseline power.rate (no panels, no robots, no humans, stasis_drain=0.05):
//   Without reactor: 0.1 + 0 - 0 - 0 - 0.05 = 0.05
//   With reactor:    0.1 + 10.0 - 0 - 0 - 0.05 = 10.05
// ===========================================================================

TEST_CASE("recompute_rates: reactor_built=false does NOT add bonus to power.rate", "[reactor][rates][build45]") {
    // Baseline: no panels, no robots, no humans, stasis_drain=0.05
    // power.rate = 0.1 + 0 - 0 - 0 - 0.05 = 0.05
    GameState s = game_init();
    s.reactor_built = false;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.05));
}

TEST_CASE("recompute_rates: reactor_built=true adds REACTOR_POWER_BONUS (10.0) to power.rate", "[reactor][rates][build45]") {
    // power.rate = 0.1 + 10.0 - 0 - 0 - 0.05 = 10.05
    GameState s = game_init();
    s.reactor_built = true;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(10.05));
}

TEST_CASE("recompute_rates: reactor bonus is exactly 10.0 above non-reactor baseline", "[reactor][rates][build45]") {
    GameState s_base = game_init();
    s_base.reactor_built = false;
    recompute_rates(s_base);
    double baseline = s_base.resources.at("power").rate;

    GameState s_reactor = game_init();
    s_reactor.reactor_built = true;
    recompute_rates(s_reactor);
    double with_bonus = s_reactor.resources.at("power").rate;

    REQUIRE((with_bonus - baseline) == Catch::Approx(10.0));
}

TEST_CASE("recompute_rates: reactor bonus stacks correctly with solar panels and robots", "[reactor][rates][build45]") {
    // 3 panels, 2 robots, reactor_built=true, no humans
    // power.rate = 0.1 + 10.0 + 3*0.75 - 2*0.2 - 0 - 0.05
    //           = 0.1 + 10.0 + 2.25 - 0.4 - 0.05 = 11.9
    GameState s = game_init();
    s.reactor_built = true;
    s.solar_panel_count = 3;
    s.resources.at("robots").value = 2.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(11.9));
}

TEST_CASE("build_reactor: power.rate reflects bonus after build (recompute_rates called)", "[reactor][rates][build45]") {
    // After build_reactor succeeds, power.rate should include the bonus
    // (build_reactor must call recompute_rates or the rate is updated by caller on next tick)
    // Spec says: "Adds a large permanent BASE_POWER_RATE bonus (flat addition to recompute_rates)"
    // We verify by calling recompute_rates explicitly after build and checking the rate.
    GameState s = game_init();
    s.current_phase = 3;
    s.resources.at("power").value = 100.0;
    build_reactor(s);
    recompute_rates(s); // explicit call to verify reactor_built triggers the bonus
    // power.rate = 0.1 + 10.0 - 0 - 0 - 0.05 = 10.05
    REQUIRE(s.resources.at("power").rate == Catch::Approx(10.05));
}

TEST_CASE("recompute_rates: reactor bonus persists across multiple calls", "[reactor][rates][build45]") {
    GameState s = game_init();
    s.reactor_built = true;
    recompute_rates(s);
    double rate1 = s.resources.at("power").rate;
    recompute_rates(s);
    double rate2 = s.resources.at("power").rate;
    REQUIRE(rate1 == Catch::Approx(rate2)); // idempotent
    REQUIRE(rate1 == Catch::Approx(10.05)); // includes 10.0 bonus
}

// ===========================================================================
// Section 5 — Save/Load edge cases
//
// New edge cases for build 45 (not covered by build44 or test_save_load.cpp):
//   - game_over=true state
//   - Combined late-game state with game_over=true and all bool flags set
// ===========================================================================

TEST_CASE("round-trip: game_over=false (default) preserved", "[save_load][game_over][build45]") {
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.game_over == false);
}

TEST_CASE("round-trip: game_over=true is preserved", "[save_load][game_over][build45]") {
    GameState s = game_init();
    s.game_over = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.game_over == true);
}

TEST_CASE("round-trip: game_over=true with stasis_pod_count=0 and awoken_humans=0 preserved", "[save_load][game_over][build45]") {
    // This is the canonical game-over terminal state
    GameState s = game_init();
    s.game_over = true;
    s.stasis_pod_count = 0;
    s.awoken_humans = 0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.game_over == true);
    REQUIRE(loaded.stasis_pod_count == 0);
    REQUIRE(loaded.awoken_humans == 0);
}

TEST_CASE("round-trip: game_tick does nothing after load when game_over=true", "[save_load][game_over][build45]") {
    // Verify that the game_over flag survives save/load and is respected by game_tick
    GameState s = game_init();
    s.game_over = true;
    s.stasis_pod_count = 0;
    s.awoken_humans = 0;
    GameState loaded = load_game(save_game(s));

    double power_before = loaded.resources.at("power").value;
    double time_before = loaded.total_time;
    loaded.pending_events.clear();
    game_tick(loaded, 10.0);

    // game_tick must return immediately when game_over=true
    REQUIRE(loaded.game_over == true);
    REQUIRE(loaded.total_time == Catch::Approx(time_before)); // time not advanced
    REQUIRE(loaded.pending_events.empty());
}

TEST_CASE("round-trip: stasis_pod_count non-default preserved", "[save_load][game_over][build45]") {
    GameState s = game_init();
    s.stasis_pod_count = 44; // 6 pods lost
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.stasis_pod_count == 44);
}

TEST_CASE("round-trip: pod_death_count preserved", "[save_load][game_over][build45]") {
    GameState s = game_init();
    s.pod_death_count = 2;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.pod_death_count == 2);
}

TEST_CASE("round-trip: power_deficit_timer preserved", "[save_load][game_over][build45]") {
    GameState s = game_init();
    s.power_deficit_timer = 15.5;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.power_deficit_timer == Catch::Approx(15.5));
}

TEST_CASE("round-trip: combined late-game state — all bool flags and edge fields preserved", "[save_load][build45]") {
    // Simulate a fully progressed game state with every new bool flag set and edge values
    GameState s = game_init();
    s.current_phase         = 3;
    s.game_over             = false; // still playing
    s.fabricator_built      = true;
    s.reactor_built         = true;
    s.robots_cap_expanded   = true;
    s.tech_scrap_vault_tier = 2;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.resources.at("robots").cap     = 10.0;
    s.resources.at("robots").value   = 10.0;
    s.solar_panel_count              = 20;
    s.solar_decay_accumulator        = 0.42;
    s.stasis_pod_count               = 30;
    s.awoken_humans                  = 10;
    s.pod_death_count                = 1;
    s.power_deficit_timer            = 0.0;
    s.total_time                     = 5000.0;
    s.scrap_boost                    = 0.03;

    GameState loaded = load_game(save_game(s));

    REQUIRE(loaded.current_phase         == 3);
    REQUIRE(loaded.game_over             == false);
    REQUIRE(loaded.fabricator_built      == true);
    REQUIRE(loaded.reactor_built         == true);
    REQUIRE(loaded.robots_cap_expanded   == true);
    REQUIRE(loaded.tech_scrap_vault_tier == 2);
    REQUIRE(loaded.resources.at("tech_scrap").cap == Catch::Approx(2000.0));
    REQUIRE(loaded.resources.at("robots").cap     == Catch::Approx(10.0));
    REQUIRE(loaded.resources.at("robots").value   == Catch::Approx(10.0));
    REQUIRE(loaded.solar_panel_count              == 20);
    REQUIRE(loaded.solar_decay_accumulator        == Catch::Approx(0.42));
    REQUIRE(loaded.stasis_pod_count               == 30);
    REQUIRE(loaded.awoken_humans                  == 10);
    REQUIRE(loaded.pod_death_count                == 1);
    REQUIRE(loaded.power_deficit_timer            == Catch::Approx(0.0));
    REQUIRE(loaded.total_time                     == Catch::Approx(5000.0));
    REQUIRE(loaded.scrap_boost                    == Catch::Approx(0.03));
}

TEST_CASE("round-trip: game_over=true combined with max vault tier and all robots", "[save_load][build45]") {
    // Terminal game-over state: every catastrophe happened
    GameState s = game_init();
    s.game_over             = true;
    s.stasis_pod_count      = 0;
    s.awoken_humans         = 0;
    s.pod_death_count       = 5;
    s.tech_scrap_vault_tier = 2;
    s.resources.at("tech_scrap").cap = 2000.0;
    s.robots_cap_expanded   = true;
    s.resources.at("robots").cap = 10.0;
    s.fabricator_built      = true;
    s.reactor_built         = true;

    GameState loaded = load_game(save_game(s));

    REQUIRE(loaded.game_over             == true);
    REQUIRE(loaded.stasis_pod_count      == 0);
    REQUIRE(loaded.awoken_humans         == 0);
    REQUIRE(loaded.pod_death_count       == 5);
    REQUIRE(loaded.tech_scrap_vault_tier == 2);
    REQUIRE(loaded.resources.at("tech_scrap").cap == Catch::Approx(2000.0));
    REQUIRE(loaded.robots_cap_expanded   == true);
    REQUIRE(loaded.resources.at("robots").cap == Catch::Approx(10.0));
    REQUIRE(loaded.fabricator_built      == true);
    REQUIRE(loaded.reactor_built         == true);
}
