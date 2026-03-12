// Build 14/17 — test_stasis_and_colony_build14.cpp
// Tests for stasis tax system, colony unit system, new resources/fields,
// new milestones, updated game_tick ordering, and save/load of all new data.
// All values derived from spec/core-spec.md and the build-14/build-17 work orders.
//
// New resources:  stasis_drain (init 0.05/cap 20.0/rate 0.0)
//                 colony_capacity (init 0.0/cap 50.0/rate 0.0)
// New fields:     colony_unit_count (init 0), stasis_pod_count (init 50)
// New functions:  buy_colony_unit(), current_colony_cost()
// New milestones: stasis_warning (stasis_drain VALUE >= 1.0)
//                 colony_started (colony_capacity VALUE >= 10.0)
// New gate:       power RATE >= 0.0, colony_capacity VALUE >= 10.0
// Constants:
//   STASIS_DRAIN_BASE=0.05, STASIS_DRAIN_GROWTH=0.003
//   STASIS_DRAIN_PER_POD=0.4, STASIS_POD_COUNT=50  (max=20.0)
//   COLONY_UNIT_BASE_COST=200.0, COLONY_UNIT_COST_SCALE=1.20
//   COLONY_CAPACITY_PER_UNIT=2

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// Helper
static Milestone* find_milestone(GameState& s, const std::string& id) {
    for (auto& m : s.milestones) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

// ===========================================================================
// Section 1 — stasis_drain resource initialization
// ===========================================================================

TEST_CASE("game_init: stasis_drain resource exists", "[stasis][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.count("stasis_drain") == 1);
}

TEST_CASE("game_init: stasis_drain.value = 0.05", "[stasis][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.05));
}

TEST_CASE("game_init: stasis_drain.cap = 4.0", "[stasis][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("stasis_drain").cap == Catch::Approx(4.0));
}

TEST_CASE("game_init: stasis_drain.rate = 0.0", "[stasis][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("stasis_drain").rate == Catch::Approx(0.0));
}

// ===========================================================================
// Section 2 — colony_capacity resource initialization
// ===========================================================================

TEST_CASE("game_init: colony_capacity resource exists", "[colony][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.count("colony_capacity") == 1);
}

TEST_CASE("game_init: colony_capacity.value = 0.0", "[colony][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(0.0));
}

TEST_CASE("game_init: colony_capacity.cap = 50.0", "[colony][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("colony_capacity").cap == Catch::Approx(50.0));
}

TEST_CASE("game_init: colony_capacity.rate = 0.0", "[colony][init]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("colony_capacity").rate == Catch::Approx(0.0));
}

// ===========================================================================
// Section 3 — new GameState fields: colony_unit_count, stasis_pod_count
// ===========================================================================

TEST_CASE("game_init: colony_unit_count = 0", "[colony][init]") {
    GameState s = game_init();
    REQUIRE(s.colony_unit_count == 0);
}

TEST_CASE("game_init: stasis_pod_count = 50", "[stasis][init]") {
    GameState s = game_init();
    REQUIRE(s.stasis_pod_count == 50);
}

// ===========================================================================
// Section 4 — current_colony_cost
// Formula: COLONY_UNIT_BASE_COST * pow(COLONY_UNIT_COST_SCALE, colony_unit_count)
//          = 200.0 * pow(1.20, count)
// ===========================================================================

TEST_CASE("current_colony_cost at count=0 returns 200.0", "[colony][cost]") {
    GameState s = game_init();
    REQUIRE(s.colony_unit_count == 0);
    REQUIRE(current_colony_cost(s) == Catch::Approx(200.0));
}

TEST_CASE("current_colony_cost at count=1 returns 240.0", "[colony][cost]") {
    // 200.0 * 1.20^1 = 240.0
    GameState s = game_init();
    s.colony_unit_count = 1;
    REQUIRE(current_colony_cost(s) == Catch::Approx(240.0));
}

TEST_CASE("current_colony_cost at count=2 returns 288.0", "[colony][cost]") {
    // 200.0 * 1.20^2 = 200.0 * 1.44 = 288.0
    GameState s = game_init();
    s.colony_unit_count = 2;
    REQUIRE(current_colony_cost(s) == Catch::Approx(288.0));
}

TEST_CASE("current_colony_cost increases monotonically with count", "[colony][cost]") {
    GameState s = game_init();
    s.colony_unit_count = 0;
    double c0 = current_colony_cost(s);
    s.colony_unit_count = 1;
    double c1 = current_colony_cost(s);
    s.colony_unit_count = 2;
    double c2 = current_colony_cost(s);
    REQUIRE(c1 > c0);
    REQUIRE(c2 > c1);
}

TEST_CASE("current_colony_cost does not modify state", "[colony][cost]") {
    GameState s = game_init();
    s.colony_unit_count = 3;
    current_colony_cost(s);
    REQUIRE(s.colony_unit_count == 3);
}

// ===========================================================================
// Section 5 — buy_colony_unit
// ===========================================================================

TEST_CASE("buy_colony_unit returns false when tech_scrap < cost", "[colony][buy]") {
    // First unit costs 20.0; tech_scrap starts at 0
    GameState s = game_init();
    bool result = buy_colony_unit(s);
    REQUIRE(result == false);
}

TEST_CASE("buy_colony_unit does not change state when cannot afford", "[colony][buy]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 19.9; // below 200.0
    buy_colony_unit(s);
    REQUIRE(s.colony_unit_count == 0);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(19.9));
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(0.0));
}

TEST_CASE("buy_colony_unit succeeds at exactly first unit cost (200.0)", "[colony][buy]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 200.0;
    bool result = buy_colony_unit(s);
    REQUIRE(result == true);
}

TEST_CASE("buy_colony_unit deducts cost from tech_scrap", "[colony][buy]") {
    // cost = 200.0; tech_scrap = 300.0 → remaining 100.0
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 300.0;
    buy_colony_unit(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(100.0));
}

TEST_CASE("buy_colony_unit increments colony_unit_count by 1", "[colony][buy]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 250.0;
    buy_colony_unit(s);
    REQUIRE(s.colony_unit_count == 1);
}

TEST_CASE("buy_colony_unit adds COLONY_CAPACITY_PER_UNIT=2 to colony_capacity.value", "[colony][buy]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 250.0;
    buy_colony_unit(s);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(2.0));
}

TEST_CASE("buy_colony_unit returns true on success", "[colony][buy]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 250.0;
    REQUIRE(buy_colony_unit(s) == true);
}

TEST_CASE("buy_colony_unit second unit uses updated cost (240.0)", "[colony][buy]") {
    // First unit: cost=200.0; second: cost=240.0
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 500.0;
    buy_colony_unit(s); // first: cost 200.0 → colony_unit_count=1, scrap=300.0
    // Second unit costs 200.0 * 1.20^1 = 240.0
    double before = s.resources.at("tech_scrap").value; // 300.0
    buy_colony_unit(s);
    REQUIRE(s.colony_unit_count == 2);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(before - 240.0)); // 60.0
}

TEST_CASE("buy_colony_unit clamps colony_capacity.value at cap 50.0", "[colony][buy]") {
    // Start colony_capacity.value near cap; COLONY_CAPACITY_PER_UNIT=2
    // 49.0 + 2.0 = 51.0 → clamped at 50.0
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 250.0;
    s.resources.at("colony_capacity").value = 49.0; // 2 more would reach 51 → clamped at 50
    buy_colony_unit(s);
    REQUIRE(s.resources.at("colony_capacity").value == Catch::Approx(50.0));
}

// ===========================================================================
// Section 6 — stasis drain update in game_tick
// Formula: stasis_drain.value = min(STASIS_DRAIN_BASE + STASIS_DRAIN_GROWTH * total_time, max)
//          max = STASIS_POD_COUNT * STASIS_DRAIN_PER_POD = 50 * 0.4 = 20.0
// Update uses total_time BEFORE it is incremented (step 1 of tick).
// ===========================================================================

TEST_CASE("game_tick: stasis_drain.value = 0.05 after first tick (t=0)", "[stasis][tick]") {
    // total_time=0 → stasis_drain = min(0.05 + 0.005*0, 2.0) = 0.05
    GameState s = game_init();
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.05));
}

TEST_CASE("game_tick: stasis_drain grows with total_time", "[stasis][tick]") {
    // Set total_time=100 then tick; stasis = min(0.05 + 0.001*100, 4.0) = min(0.15, 4.0) = 0.15
    GameState s = game_init();
    s.total_time = 100.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.15));
}

TEST_CASE("game_tick: stasis_drain uses total_time BEFORE increment", "[stasis][tick]") {
    // total_time=0; tick with delta=10.0; stasis uses t=0 → 0.05
    // After tick total_time becomes 10.0, but stasis was set using t=0
    GameState s = game_init();
    game_tick(s, 10.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.05));
    // Second tick: uses total_time=10 → min(0.05+0.001*10, 4.0) = 0.06
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.06));
}

TEST_CASE("game_tick: stasis_drain capped at 4.0 (STASIS_DRAIN_CAP)", "[stasis][tick]") {
    // At t=33400: min(0.05 + 0.001*33400, 50*2.0, 4.0) = min(33.45, 100.0, 4.0) = 4.0
    // STASIS_DRAIN_CAP = 4.0 applies as hard ceiling
    GameState s = game_init();
    s.total_time = 33400.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(4.0));
}

TEST_CASE("game_tick: stasis_drain update happens before recompute_rates (power.rate reflects new drain)", "[stasis][tick]") {
    // At total_time=190: stasis_drain = min(0.05+0.001*190, 4.0) = min(0.24, 4.0) = 0.24
    // After recompute: power.rate = 0.1 + 0 - 0 - 0.24 = -0.14
    GameState s = game_init();
    s.total_time = 190.0;
    game_tick(s, 0.0);
    REQUIRE(s.resources.at("stasis_drain").value == Catch::Approx(0.24));
    REQUIRE(s.resources.at("power").rate == Catch::Approx(-0.14));
}

// ===========================================================================
// Section 7 — new milestones: stasis_warning and colony_started
// ===========================================================================

TEST_CASE("game_init: total milestones count is 7", "[milestones][init]") {
    GameState s = game_init();
    REQUIRE(s.milestones.size() == 7);
}

TEST_CASE("game_init: stasis_warning milestone exists", "[milestones][init]") {
    GameState s = game_init();
    REQUIRE(find_milestone(s, "stasis_warning") != nullptr);
}

TEST_CASE("game_init: colony_started milestone exists", "[milestones][init]") {
    GameState s = game_init();
    REQUIRE(find_milestone(s, "colony_started") != nullptr);
}

TEST_CASE("game_init: stasis_warning condition_resource = stasis_drain", "[milestones][init]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "stasis_warning");
    REQUIRE(m != nullptr);
    REQUIRE(m->condition_resource == "stasis_drain");
}

TEST_CASE("game_init: stasis_warning condition_value = 1.0", "[milestones][init]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "stasis_warning");
    REQUIRE(m != nullptr);
    REQUIRE(m->condition_value == Catch::Approx(1.0));
}

TEST_CASE("game_init: stasis_warning condition_field = VALUE", "[milestones][init]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "stasis_warning");
    REQUIRE(m != nullptr);
    REQUIRE(m->condition_field == ConditionField::VALUE);
}

TEST_CASE("game_init: colony_started condition_resource = colony_capacity", "[milestones][init]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "colony_started");
    REQUIRE(m != nullptr);
    REQUIRE(m->condition_resource == "colony_capacity");
}

TEST_CASE("game_init: colony_started condition_value = 2.0", "[milestones][init]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "colony_started");
    REQUIRE(m != nullptr);
    REQUIRE(m->condition_value == Catch::Approx(2.0));
}

TEST_CASE("game_init: colony_started condition_field = VALUE", "[milestones][init]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "colony_started");
    REQUIRE(m != nullptr);
    REQUIRE(m->condition_field == ConditionField::VALUE);
}

TEST_CASE("check_milestones: stasis_warning not triggered below 1.0", "[milestones][stasis]") {
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 0.9;
    check_milestones(s);
    Milestone* m = find_milestone(s, "stasis_warning");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
}

TEST_CASE("check_milestones: stasis_warning triggers at exactly 1.0", "[milestones][stasis]") {
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 1.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "stasis_warning");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

TEST_CASE("check_milestones: stasis_warning fires event at 1.0", "[milestones][stasis]") {
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 1.0;
    check_milestones(s);
    bool found = false;
    for (const auto& e : s.pending_events) {
        if (e == "stasis_warning") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("check_milestones: colony_started not triggered below 2.0", "[milestones][colony]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 1.9;
    check_milestones(s);
    Milestone* m = find_milestone(s, "colony_started");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
}

TEST_CASE("check_milestones: colony_started triggers at exactly 2.0", "[milestones][colony]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 2.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "colony_started");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

TEST_CASE("check_milestones: colony_started fires event at 2.0", "[milestones][colony]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 2.0;
    check_milestones(s);
    bool found = false;
    for (const auto& e : s.pending_events) {
        if (e == "colony_started") found = true;
    }
    REQUIRE(found);
}

// ===========================================================================
// Section 8 — GateCondition condition_field dispatch (VALUE vs RATE)
// ===========================================================================

TEST_CASE("check_phase_gates: RATE condition checks rate not value", "[gates][dispatch]") {
    // Gate requires power RATE >= 0.0
    // Set power.value=100 (would pass VALUE check) but power.rate=-1.0 (fails RATE check)
    GameState s = game_init();
    s.phase_gates.clear();
    PhaseGate gate;
    gate.phase = 2;
    GateCondition rate_cond;
    rate_cond.resource_id     = "power";
    rate_cond.min_value       = 0.0;
    rate_cond.condition_field = ConditionField::RATE;
    gate.conditions.push_back(rate_cond);
    s.phase_gates.push_back(gate);
    s.resources.at("power").value = 100.0; // would satisfy VALUE check
    s.resources.at("power").rate  = -1.0;  // fails RATE check
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1); // should not advance
}

TEST_CASE("check_phase_gates: RATE condition advances when rate meets threshold", "[gates][dispatch]") {
    // Gate requires power RATE >= 0.0; set rate=0.5, value=0 (would fail VALUE check)
    GameState s = game_init();
    s.phase_gates.clear();
    PhaseGate gate;
    gate.phase = 2;
    GateCondition rate_cond;
    rate_cond.resource_id     = "power";
    rate_cond.min_value       = 0.0;
    rate_cond.condition_field = ConditionField::RATE;
    gate.conditions.push_back(rate_cond);
    s.phase_gates.push_back(gate);
    s.resources.at("power").value = 0.0;  // would fail VALUE check
    s.resources.at("power").rate  = 0.5;  // meets RATE check
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2); // should advance
}

TEST_CASE("check_phase_gates: VALUE condition checks value not rate", "[gates][dispatch]") {
    // Gate requires colony_capacity VALUE >= 10.0
    // Set colony_capacity.rate=100 (would pass RATE check) but value=5.0 (fails VALUE check)
    GameState s = game_init();
    s.phase_gates.clear();
    PhaseGate gate;
    gate.phase = 2;
    GateCondition val_cond;
    val_cond.resource_id     = "colony_capacity";
    val_cond.min_value       = 10.0;
    val_cond.condition_field = ConditionField::VALUE;
    gate.conditions.push_back(val_cond);
    s.phase_gates.push_back(gate);
    s.resources.at("colony_capacity").rate  = 100.0; // would satisfy RATE check
    s.resources.at("colony_capacity").value = 5.0;   // fails VALUE check
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1); // should not advance
}

// ===========================================================================
// Section 9 — save/load round-trip for new fields
// ===========================================================================

TEST_CASE("round-trip: all six resources are present after load", "[save_load][resources]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.resources.count("power")           == 1);
    REQUIRE(loaded.resources.count("compute")         == 1);
    REQUIRE(loaded.resources.count("robots")          == 1);
    REQUIRE(loaded.resources.count("tech_scrap")      == 1);
    REQUIRE(loaded.resources.count("stasis_drain")    == 1);
    REQUIRE(loaded.resources.count("colony_capacity") == 1);
}

TEST_CASE("round-trip: stasis_drain.value is preserved", "[save_load][stasis]") {
    GameState s = game_init();
    s.resources.at("stasis_drain").value = 1.5;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("stasis_drain").value == Catch::Approx(1.5));
}

TEST_CASE("round-trip: stasis_drain.cap is preserved", "[save_load][stasis]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.resources.at("stasis_drain").cap == Catch::Approx(4.0));
}

TEST_CASE("round-trip: stasis_drain.rate is preserved", "[save_load][stasis]") {
    GameState s = game_init();
    s.resources.at("stasis_drain").rate = 0.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("stasis_drain").rate == Catch::Approx(0.0));
}

TEST_CASE("round-trip: colony_capacity.value is preserved", "[save_load][colony]") {
    GameState s = game_init();
    s.resources.at("colony_capacity").value = 30.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("colony_capacity").value == Catch::Approx(30.0));
}

TEST_CASE("round-trip: colony_capacity.cap is preserved", "[save_load][colony]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.resources.at("colony_capacity").cap == Catch::Approx(50.0));
}

TEST_CASE("round-trip: colony_unit_count default (0) is preserved", "[save_load][colony]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.colony_unit_count == 0);
}

TEST_CASE("round-trip: colony_unit_count non-default is preserved", "[save_load][colony]") {
    GameState s = game_init();
    s.colony_unit_count = 3;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.colony_unit_count == 3);
}

TEST_CASE("round-trip: stasis_pod_count default (50) is preserved", "[save_load][stasis]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.stasis_pod_count == 50);
}

TEST_CASE("round-trip: stasis_pod_count non-default is preserved", "[save_load][stasis]") {
    GameState s = game_init();
    s.stasis_pod_count = 25;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.stasis_pod_count == 25);
}

TEST_CASE("round-trip: stasis_warning milestone id is present after load", "[save_load][milestones]") {
    GameState loaded = load_game(save_game(game_init()));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "stasis_warning") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: colony_started milestone id is present after load", "[save_load][milestones]") {
    GameState loaded = load_game(save_game(game_init()));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "colony_started") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: stasis_warning triggered=true is preserved", "[save_load][milestones]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "stasis_warning");
    REQUIRE(m != nullptr);
    m->triggered = true;
    GameState loaded = load_game(save_game(s));
    Milestone* lm = find_milestone(loaded, "stasis_warning");
    REQUIRE(lm != nullptr);
    REQUIRE(lm->triggered == true);
}

TEST_CASE("round-trip: colony_started triggered=true is preserved", "[save_load][milestones]") {
    GameState s = game_init();
    Milestone* m = find_milestone(s, "colony_started");
    REQUIRE(m != nullptr);
    m->triggered = true;
    GameState loaded = load_game(save_game(s));
    Milestone* lm = find_milestone(loaded, "colony_started");
    REQUIRE(lm != nullptr);
    REQUIRE(lm->triggered == true);
}
