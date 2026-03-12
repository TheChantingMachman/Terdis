// Build 49 — test_build49.cpp
// New tests for build 49 scope:
//   C1: SOLAR_POWER_RATE 0.5 → 0.75
//   C2: STASIS_DRAIN_GROWTH 0.003 → 0.001
//   C3: STASIS_DRAIN_CAP = 4.0 (new hard ceiling on stasis drain)
//
// Formula: stasis_drain.value = min(BASE + GROWTH*t, pod_count*PER_POD, STASIS_DRAIN_CAP)
//
// All values derived from spec/core-spec.md.
//
// Constants:
//   STASIS_DRAIN_BASE    = 0.05
//   STASIS_DRAIN_GROWTH  = 0.001
//   STASIS_DRAIN_PER_POD = 2.0
//   STASIS_DRAIN_CAP     = 4.0
//   SOLAR_POWER_RATE     = 0.75
//   BASE_POWER_RATE      = 0.1
//   ROBOT_POWER_DRAIN    = 0.2

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------
static int count_event49(const GameState& s, const std::string& id) {
    int n = 0;
    for (const auto& ev : s.pending_events) {
        if (ev == id) n++;
    }
    return n;
}

// ===========================================================================
// Section 1 — STASIS_DRAIN_CAP = 4.0 hard ceiling
// ===========================================================================

TEST_CASE("STASIS_DRAIN_CAP: limits drain to 4.0 at high total_time",
          "[stasis][cap][build49]") {
    // min(0.05 + 0.001*10000, 50*2.0, 4.0) = min(10.05, 100.0, 4.0) = 4.0
    GameState s = game_init();
    s.total_time = 10000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

TEST_CASE("STASIS_DRAIN_CAP: reached at exactly t=3950",
          "[stasis][cap][build49]") {
    // 0.05 + 0.001*3950 = 4.0 — exactly at cap
    GameState s = game_init();
    s.total_time = 3950.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

TEST_CASE("STASIS_DRAIN_CAP: not yet reached at t=3949",
          "[stasis][cap][build49]") {
    // 0.05 + 0.001*3949 = 3.999 < 4.0
    GameState s = game_init();
    s.total_time = 3949.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(3.999));
}

TEST_CASE("STASIS_DRAIN_CAP: applies even when pod cap is higher",
          "[stasis][cap][build49]") {
    // pod_count=50 → pod_cap=50*2.0=100.0; STASIS_DRAIN_CAP=4.0
    // min(0.05+0.001*5000, 100.0, 4.0) = min(5.05, 100.0, 4.0) = 4.0
    GameState s = game_init();
    s.stasis_pod_count = 50;
    s.total_time = 5000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

TEST_CASE("pod cap still applies when lower than STASIS_DRAIN_CAP",
          "[stasis][cap][build49]") {
    // pod_count=1 → pod_cap=1*2.0=2.0; STASIS_DRAIN_CAP=4.0
    // min(0.05+0.001*5000, 2.0, 4.0) = min(5.05, 2.0, 4.0) = 2.0 (pod cap wins)
    GameState s = game_init();
    s.stasis_pod_count = 1;
    s.total_time = 5000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(2.0));
}

TEST_CASE("STASIS_DRAIN_CAP with pod_count=2: pod cap equals STASIS_DRAIN_CAP",
          "[stasis][cap][build49]") {
    // pod_count=2 → pod_cap=2*2.0=4.0; STASIS_DRAIN_CAP=4.0
    // min(0.05+0.001*10000, 4.0, 4.0) = min(10.05, 4.0, 4.0) = 4.0
    GameState s = game_init();
    s.stasis_pod_count = 2;
    s.total_time = 10000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

// ===========================================================================
// Section 2 — game_init: stasis_drain.max updated to 4.0
// ===========================================================================

TEST_CASE("game_init: stasis_drain resource max is 4.0",
          "[stasis][init][build49]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("stasis_drain").max == Catch::Approx(4.0));
}

// ===========================================================================
// Section 3 — SOLAR_POWER_RATE = 0.75 verified through recompute_rates
// ===========================================================================

TEST_CASE("SOLAR_POWER_RATE=0.75: verified with 1 panel through recompute_rates",
          "[rates][solar][build49]") {
    // power.rate = 0.1 + 1*0.75 - 0 - 0.05 = 0.80
    GameState s = game_init();
    s.solar_panel_count = 1;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.80));
}

TEST_CASE("SOLAR_POWER_RATE=0.75: 3 panels, 1 robot → power.rate = 2.0",
          "[rates][solar][build49]") {
    // power.rate = 0.1 + 3*0.75 - 1*0.2 - 0.05 = 0.1 + 2.25 - 0.2 - 0.05 = 2.1
    GameState s = game_init();
    s.solar_panel_count = 3;
    s.resources.at("robots").value = 1.0;
    recompute_rates(s);
    REQUIRE(s.resources.at("power").rate == Catch::Approx(2.1));
}

// ===========================================================================
// Section 4 — STASIS_DRAIN_GROWTH = 0.001 verified
// ===========================================================================

TEST_CASE("STASIS_DRAIN_GROWTH=0.001: verified at t=500",
          "[stasis][growth][build49]") {
    // 0.05 + 0.001*500 = 0.55
    GameState s = game_init();
    s.total_time = 500.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.55));
}

TEST_CASE("STASIS_DRAIN_GROWTH=0.001: verified at t=1000",
          "[stasis][growth][build49]") {
    // 0.05 + 0.001*1000 = 1.05
    GameState s = game_init();
    s.total_time = 1000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(1.05));
}

// ===========================================================================
// Section 5 — stasis_warning milestone timing (new: t=950)
// ===========================================================================

TEST_CASE("stasis_warning milestone does NOT fire at t=949",
          "[milestone][stasis_warning][build49]") {
    // 0.05 + 0.001*949 = 0.999 < 1.0 → NOT triggered
    GameState s = game_init();
    s.total_time = 949.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.999));
    REQUIRE(count_event49(s, "stasis_warning") == 0);
}

TEST_CASE("stasis_warning milestone fires at t=950",
          "[milestone][stasis_warning][build49]") {
    // 0.05 + 0.001*950 = 1.0 >= 1.0 → TRIGGERED
    GameState s = game_init();
    s.total_time = 950.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(1.0));
    REQUIRE(count_event49(s, "stasis_warning") == 1);
    // Verify triggered flag
    bool triggered = false;
    for (const auto& m : s.milestones) {
        if (m.id == "stasis_warning") triggered = m.triggered;
    }
    REQUIRE(triggered == true);
}

// ===========================================================================
// Section 6 — Save/load preserves STASIS_DRAIN_CAP behaviour
// ===========================================================================

TEST_CASE("save/load preserves STASIS_DRAIN_CAP behaviour",
          "[stasis][cap][save_load][build49]") {
    // After save/load, drain at t=5000 must still be capped at 4.0
    GameState s = game_init();
    s.total_time = 5000.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0)); // precondition

    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

TEST_CASE("save/load: stasis_drain.max=4.0 is preserved",
          "[stasis][cap][save_load][build49]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.resources.at("stasis_drain").max == Catch::Approx(4.0));
}
